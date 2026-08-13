#include <map>
#include <mutex>
#include <algorithm>

#include <woki/asset/manager.hpp>

namespace woki::asset {

class AssetManager::Impl {
public:
    struct Record {
        AssetState state{AssetState::Unloaded};
        u64 revision{};
        u64 generation{};
        ref<const AssetGeneration> current;
        std::optional<AssetError> error;
        std::optional<task::Future<AssetLease>> in_flight;
        task::CancellationSource cancellation;
        std::vector<task::CancellationToken> waiters;
        AssetPriority priority{AssetPriority::Normal};
        bool prefetch{};
    };

    Impl(Load loader, AssetManagerOptions options)
        : load(std::move(loader)),
          scheduler(options.worker_count, options.queue_capacity),
          prefetch_io(options.io_workers, options.queue_capacity),
          normal_io(options.io_workers, options.queue_capacity),
          high_io(options.io_workers, options.queue_capacity),
          publications(options.publication_capacity),
          max_decoded_bytes(options.max_decoded_bytes),
          max_waiters_per_asset(options.max_waiters_per_asset),
          max_prefetch_requests(options.max_prefetch_requests) {}

    Load load;
    task::Scheduler scheduler;
    task::IoExecutor prefetch_io;
    task::IoExecutor normal_io;
    task::IoExecutor high_io;
    task::CompletionQueue publications;
    std::size_t max_decoded_bytes;
    std::size_t max_waiters_per_asset;
    std::size_t max_prefetch_requests;
    mutable std::mutex mutex;
    std::map<AssetId, Record> records;
    std::size_t resident_bytes{};
    std::size_t prefetch_requests{};
};

AssetManager::AssetManager(Load load, AssetManagerOptions options)
    : impl_(createScope<Impl>(std::move(load), options)) {}

AssetManager::~AssetManager() {
    impl_->prefetch_io.RequestStop();
    impl_->normal_io.RequestStop();
    impl_->high_io.RequestStop();
    impl_->scheduler.RequestStop();
    impl_->prefetch_io.Join();
    impl_->normal_io.Join();
    impl_->high_io.Join();
    impl_->scheduler.Join();
    impl_->publications.RequestStop();
}

Result<task::Future<AssetLease>> AssetManager::Request(const AssetId id, const task::CancellationToken cancellation) {
    return Request(id, AssetRequestOptions{.cancellation = cancellation});
}

Result<task::Future<AssetLease>> AssetManager::Request(const AssetId id, AssetRequestOptions options) {
    if (!id)
        return Err(ErrorCode::InvalidArgument, "cannot request a nil asset ID");
    if (options.cancellation.IsCancellationRequested())
        return Err(ErrorCode::Cancelled, "asset request was cancelled");
    u64 revision{};
    task::CancellationToken operation_token;
    {
        std::lock_guard lock(impl_->mutex);
        auto& record = impl_->records[id];
        if (record.in_flight) {
            if (record.waiters.size() >= impl_->max_waiters_per_asset)
                return Err(ErrorCode::QueueFull, "asset waiter limit exceeded");
            record.waiters.push_back(options.cancellation);
            record.priority = std::max(record.priority, options.priority);
            auto waiter = record.in_flight->Then(impl_->publications, [cancellation = options.cancellation](const Result<AssetLease>& result) -> Result<AssetLease> {
                if (cancellation.IsCancellationRequested())
                    return Err(ErrorCode::Cancelled, "asset request waiter was cancelled");
                return result;
            });
            return Ok(std::move(waiter));
        }
        if (record.current && record.state == AssetState::Ready)
            return task::Schedule(impl_->publications, [lease = AssetLease(record.current), cancellation = options.cancellation]() -> Result<AssetLease> {
                return cancellation.IsCancellationRequested() ? Result<AssetLease>(Err(ErrorCode::Cancelled, "asset request waiter was cancelled")) : Ok(lease);
            });
        if (options.prefetch && impl_->prefetch_requests >= impl_->max_prefetch_requests)
            return Err(ErrorCode::QueueFull, "asset prefetch queue is full");
        record.state = AssetState::Queued;
        record.error.reset();
        revision = record.revision;
        record.cancellation = task::CancellationSource{};
        record.waiters = {options.cancellation};
        record.priority = options.priority;
        record.prefetch = options.prefetch;
        if (record.prefetch)
            ++impl_->prefetch_requests;
        operation_token = record.cancellation.Token();
    }
    task::IoExecutor* lane = options.priority == AssetPriority::High ? &impl_->high_io : options.priority == AssetPriority::Normal ? &impl_->normal_io : &impl_->prefetch_io;
    auto loaded = task::Schedule(
        *lane,
        [this, id, operation_token]() -> Result<Product> {
            if (operation_token.IsCancellationRequested())
                return Err(ErrorCode::Cancelled, "asset load was invalidated");
            {
                std::lock_guard lock(impl_->mutex);
                const auto found = impl_->records.find(id);
                if (found == impl_->records.end() || (!found->second.prefetch && std::ranges::all_of(found->second.waiters, &task::CancellationToken::IsCancellationRequested))) {
                    if (found != impl_->records.end())
                        (void)found->second.cancellation.RequestCancellation();
                    return Err(ErrorCode::Cancelled, "all asset request waiters were cancelled");
                }
            }
            return impl_->load(id, operation_token);
        },
        operation_token
    );
    if (!loaded) {
        std::lock_guard lock(impl_->mutex);
        auto& record = impl_->records[id];
        if (record.prefetch) {
            --impl_->prefetch_requests;
            record.prefetch = false;
        }
        record.state = AssetState::Failed;
        record.error = AssetError{loaded.error().Code(), std::string(loaded.error().Message())};
        return Err(std::move(loaded).error());
    }
    auto validated = loaded->Then(impl_->scheduler, [this](const Result<Product>& result) -> Result<Product> {
        if (!result)
            return Err(result.error());
        TRY_VOID(ValidateProduct(*result));
        if (result->payload.size() > impl_->max_decoded_bytes)
            return Err(ErrorCode::OutOfRange, "asset exceeds decoded-byte backpressure limit");
        return Ok(*result);
    });
    auto published = validated.Then(impl_->publications, [this, id, revision](const Result<Product>& result) -> Result<AssetLease> {
        std::lock_guard lock(impl_->mutex);
        auto found = impl_->records.find(id);
        if (found == impl_->records.end() || found->second.revision != revision)
            return Err(ErrorCode::Cancelled, "asset publication is stale");
        auto& record = found->second;
        record.in_flight.reset();
        record.waiters.clear();
        if (record.prefetch) {
            --impl_->prefetch_requests;
            record.prefetch = false;
        }
        if (!result) {
            record.state = AssetState::Failed;
            record.error = AssetError{result.error().Code(), std::string(result.error().Message())};
            return Err(result.error());
        }
        if (result->asset_id != id) {
            record.state = AssetState::Failed;
            record.error = AssetError{ErrorCode::ValidationInvalidState, "loaded product identity does not match request"};
            return Err(ErrorCode::ValidationInvalidState, "loaded product identity does not match request");
        }
        const std::size_t previous = record.current ? record.current->bytes.size() : 0;
        if (impl_->resident_bytes - previous + result->payload.size() > impl_->max_decoded_bytes) {
            record.state = AssetState::Failed;
            record.error = AssetError{ErrorCode::QueueFull, "asset decoded-byte budget is full"};
            return Err(ErrorCode::QueueFull, "asset decoded-byte budget is full");
        }
        ++record.generation;
        auto generation = createRef<const AssetGeneration>(AssetGeneration{{id, result->subresource}, result->type, {record.revision, record.generation, result->product_hash}, result->payload});
        impl_->resident_bytes = impl_->resident_bytes - previous + generation->bytes.size();
        record.current = generation;
        record.state = AssetState::Ready;
        record.error.reset();
        return Ok(AssetLease(std::move(generation)));
    });
    {
        std::lock_guard lock(impl_->mutex);
        auto& record = impl_->records[id];
        if (record.revision != revision)
            return Err(ErrorCode::Cancelled, "asset request was invalidated before submission");
        record.state = AssetState::Loading;
        record.in_flight = published;
    }
    auto waiter = published.Then(impl_->publications, [cancellation = options.cancellation](const Result<AssetLease>& result) -> Result<AssetLease> {
        if (cancellation.IsCancellationRequested())
            return Err(ErrorCode::Cancelled, "asset request waiter was cancelled");
        return result;
    });
    return Ok(std::move(waiter));
}

Result<void> AssetManager::Prefetch(const AssetId id, const AssetPriority priority) {
    auto requested = Request(id, AssetRequestOptions{.cancellation = {}, .priority = priority, .prefetch = true});
    return requested ? Ok() : Err(std::move(requested).error());
}

std::optional<AssetLease> AssetManager::Borrow(const AssetId id) const {
    std::lock_guard lock(impl_->mutex);
    const auto found = impl_->records.find(id);
    return found == impl_->records.end() || !found->second.current ? std::nullopt : std::optional(AssetLease(found->second.current));
}

AssetStatus AssetManager::Status(const AssetId id) const {
    std::lock_guard lock(impl_->mutex);
    const auto found = impl_->records.find(id);
    if (found == impl_->records.end())
        return {};
    const auto& record = found->second;
    return {record.state, {record.revision, record.generation, record.current ? record.current->version.product_hash : ContentHash{}}, record.error, record.in_flight.has_value()};
}

void AssetManager::Invalidate(const AssetId id) {
    std::lock_guard lock(impl_->mutex);
    auto& record = impl_->records[id];
    ++record.revision;
    (void)record.cancellation.RequestCancellation();
    record.in_flight.reset();
    record.state = AssetState::Unloaded;
}

void AssetManager::Invalidate(const std::span<const AssetId> ids) {
    for (const auto id : ids)
        Invalidate(id);
}

std::size_t AssetManager::ReleaseUnused() {
    std::lock_guard lock(impl_->mutex);
    std::size_t count{};
    for (auto it = impl_->records.begin(); it != impl_->records.end();) {
        auto& record = it->second;
        if (record.current && record.current.use_count() == 1 && !record.in_flight) {
            impl_->resident_bytes -= record.current->bytes.size();
            record.current.reset();
            record.state = AssetState::Unloaded;
            ++count;
        }
        if (!record.current && !record.in_flight && record.revision == 0)
            it = impl_->records.erase(it);
        else
            ++it;
    }
    return count;
}

std::size_t AssetManager::PumpPublications(const std::size_t limit) {
    return impl_->publications.Drain(limit);
}

Result<std::vector<AssetLease>> AssetManager::PublishAtomic(const std::span<const Product> products) {
    if (products.empty())
        return Err(ErrorCode::InvalidArgument, "asset publication transaction is empty");
    std::lock_guard lock(impl_->mutex);
    std::map<AssetId, const Product*> batch;
    for (const auto& product : products) {
        TRY_VOID(ValidateProduct(product));
        if (!batch.emplace(product.asset_id, &product).second)
            return Err(ErrorCode::ValidationInvalidState, "asset publication transaction contains duplicate IDs");
    }
    for (const auto& product : products)
        for (const auto& dependency : product.dependencies) {
            if (const auto pending = batch.find(dependency.asset_id); pending != batch.end()) {
                if (pending->second->product_hash != dependency.product_hash)
                    return Err(ErrorCode::ValidationInvalidState, "transaction dependency hash does not match its generated product");
            } else {
                const auto current = impl_->records.find(dependency.asset_id);
                // Source/external package dependencies need not be resident in
                // this manager. If they are resident, their identity must agree.
                if (current != impl_->records.end() && current->second.current != nullptr && current->second.current->version.product_hash != dependency.product_hash)
                    return Err(ErrorCode::ValidationInvalidState, "asset publication transaction dependency hash conflicts with the resident generation");
            }
        }
    std::size_t admitted = impl_->resident_bytes;
    for (const auto& product : products) {
        const auto current = impl_->records.find(product.asset_id);
        if (current != impl_->records.end() && current->second.current)
            admitted -= current->second.current->bytes.size();
        if (product.payload.size() > impl_->max_decoded_bytes || admitted > impl_->max_decoded_bytes - product.payload.size())
            return Err(ErrorCode::QueueFull, "asset publication transaction exceeds the decoded-byte budget");
        admitted += product.payload.size();
    }
    std::vector<AssetLease> leases;
    leases.reserve(products.size());
    for (const auto& product : products) {
        auto& record = impl_->records[product.asset_id];
        (void)record.cancellation.RequestCancellation();
        record.in_flight.reset();
        if (record.prefetch) {
            --impl_->prefetch_requests;
            record.prefetch = false;
        }
        ++record.generation;
        auto generation = createRef<const AssetGeneration>(AssetGeneration{{product.asset_id, product.subresource}, product.type, {record.revision, record.generation, product.product_hash}, product.payload});
        record.current = generation;
        record.state = AssetState::Ready;
        record.error.reset();
        leases.emplace_back(std::move(generation));
    }
    impl_->resident_bytes = admitted;
    return Ok(std::move(leases));
}

} // namespace woki::asset
