#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>

#include <catch2/catch_test_macros.hpp>

#include <woki/asset.hpp>

#include "test_support.hpp"

using namespace std::chrono_literals;
using namespace woki;
using namespace woki::asset;

namespace {
class LoadGate {
public:
    void Wait() {
        std::unique_lock lock(mutex_);
        entered_ = true;
        condition_.notify_all();
        condition_.wait(lock, [&] { return open_; });
    }

    bool WaitForEntry() {
        std::unique_lock lock(mutex_);
        return condition_.wait_for(lock, 2s, [&] { return entered_; });
    }

    void Open() {
        std::lock_guard lock(mutex_);
        open_ = true;
        condition_.notify_all();
    }

private:
    std::mutex mutex_;
    std::condition_variable condition_;
    bool entered_{};
    bool open_{};
};
} // namespace

TEST_CASE("AssetManager coalesces loads while cancellation remains waiter-local") {
    const auto id = AssetId::FromName("coalesced");
    std::atomic_uint loads{};
    AssetManager manager([&](AssetId requested, task::CancellationToken) {
        ++loads;
        return Ok(test::ProductFor(requested, "coalesced"));
    });
    task::CancellationSource cancelled_waiter;
    auto first = manager.Request(id, cancelled_waiter.Token());
    auto second = manager.Request(id);
    REQUIRE(first);
    REQUIRE(second);
    REQUIRE(cancelled_waiter.RequestCancellation());
    REQUIRE(test::PumpUntil(manager, *first));
    REQUIRE(test::PumpUntil(manager, *second));
    REQUIRE(loads.load() == 1);
    REQUIRE(first->Wait().error().Code() == ErrorCode::Cancelled);
    REQUIRE(second->Wait());
    REQUIRE(manager.Status(id).state == AssetState::Ready);
    REQUIRE(manager.Borrow(id));
}

TEST_CASE("AssetManager reload publishes a new generation while old leases stay stable") {
    const auto id = AssetId::FromName("reload-generation");
    std::atomic_uint loads{};
    AssetManager manager([&](AssetId requested, task::CancellationToken) {
        const auto value = ++loads;
        return Ok(test::ProductFor(requested, value == 1 ? "first" : "second"));
    });
    auto first_future = manager.Request(id);
    REQUIRE(first_future);
    REQUIRE(test::PumpUntil(manager, *first_future));
    const auto first = first_future->Wait();
    REQUIRE(first);
    REQUIRE(first->Get().version.generation == 1);

    manager.Invalidate(id);
    auto second_future = manager.Request(id);
    REQUIRE(second_future);
    REQUIRE(test::PumpUntil(manager, *second_future));
    const auto second = second_future->Wait();
    REQUIRE(second);
    REQUIRE(second->Get().version.revision == 1);
    REQUIRE(second->Get().version.generation == 2);
    REQUIRE_FALSE(std::ranges::equal(first->Bytes(), second->Bytes()));
    REQUIRE(first->Get().version.generation == 1);
}

TEST_CASE("AssetManager enforces waiter prefetch publication and resident bounds") {
    const auto id = AssetId::FromName("bounds");
    AssetManagerOptions options;
    options.max_waiters_per_asset = 0;
    options.max_prefetch_requests = 0;
    options.max_decoded_bytes = 3;
    options.publication_capacity = 8;
    AssetManager manager([](AssetId requested, task::CancellationToken) { return Ok(test::ProductFor(requested, "four")); }, options);
    auto first = manager.Request(id);
    REQUIRE(first);
    REQUIRE_FALSE(manager.Request(id));
    REQUIRE_FALSE(manager.Prefetch(AssetId::FromName("prefetch")));
    REQUIRE(test::PumpUntil(manager, *first));
    REQUIRE(first->Wait().error().Code() == ErrorCode::OutOfRange);
    REQUIRE(manager.Status(id).state == AssetState::Failed);

    const auto small = test::ProductFor(AssetId::FromName("small"), "1234");
    REQUIRE_FALSE(manager.PublishAtomic(std::array{small}));
    REQUIRE_FALSE(manager.PublishAtomic(std::span<const Product>{}));
}

#ifndef __EMSCRIPTEN__
TEST_CASE("high priority requests use an independent I/O lane") {
    const auto normal_id = AssetId::FromName("normal-lane");
    const auto high_id = AssetId::FromName("high-lane");
    LoadGate normal_gate;
    AssetManagerOptions options;
    options.worker_count = 1;
    options.io_workers = 1;
    options.queue_capacity = 4;
    AssetManager manager(
        [&](AssetId requested, task::CancellationToken) {
            if (requested == normal_id)
                normal_gate.Wait();
            return Ok(test::ProductFor(requested));
        },
        options
    );
    auto normal = manager.Request(normal_id, AssetRequestOptions{.priority = AssetPriority::Normal});
    REQUIRE(normal);
    const bool entered = normal_gate.WaitForEntry();
    if (!entered) {
        normal_gate.Open();
        REQUIRE(entered);
    }
    auto high = manager.Request(high_id, AssetRequestOptions{.priority = AssetPriority::High});
    const bool high_ready = high && test::PumpUntil(manager, *high);
    const bool normal_was_pending = !normal->IsReady();
    normal_gate.Open();
    REQUIRE(test::PumpUntil(manager, *normal));
    REQUIRE(high);
    REQUIRE(high_ready);
    REQUIRE(high->Wait());
    REQUIRE(normal_was_pending);
    REQUIRE(normal->Wait());
}

TEST_CASE("invalidating an in-flight load prevents stale publication") {
    const auto id = AssetId::FromName("stale-publication");
    LoadGate gate;
    std::atomic_uint calls{};
    AssetManager manager([&](AssetId requested, task::CancellationToken) {
        if (++calls == 1)
            gate.Wait();
        return Ok(test::ProductFor(requested, calls == 1 ? "stale" : "fresh"));
    });
    auto stale = manager.Request(id);
    REQUIRE(stale);
    const bool entered = gate.WaitForEntry();
    manager.Invalidate(id);
    gate.Open();
    REQUIRE(entered);
    REQUIRE(test::PumpUntil(manager, *stale));
    REQUIRE(stale->Wait().error().Code() == ErrorCode::Cancelled);
    REQUIRE_FALSE(manager.Borrow(id));

    auto fresh = manager.Request(id);
    REQUIRE(fresh);
    REQUIRE(test::PumpUntil(manager, *fresh));
    REQUIRE(fresh->Wait());
    REQUIRE(manager.Status(id).version.revision == 1);
}
#endif

TEST_CASE("ReloadCoordinator invalidates changed assets and transitive dependents") {
    const auto source_id = AssetId::FromName("reload-source");
    const auto dependent_id = AssetId::FromName("reload-dependent");
    const auto path = AssetPath::Parse("source.txt");
    const auto uri = AssetUri::Parse("project://source.txt");
    REQUIRE(path);
    REQUIRE(uri);
    const auto mount = createRef<MemoryMount>();
    mount->PutText(*path, "new");
    Vfs vfs;
    REQUIRE(vfs.MountAt("project", AssetScheme::Project, {}, 0, mount));

    AssetDatabase database;
    auto transaction = database.BeginTransaction();
    REQUIRE(transaction.Upsert({source_id, *uri, 1, 1, Sha256("old"), {}, 4, 1}));
    REQUIRE(transaction.Commit());
    DependencyGraph dependencies;
    const ProductDependency source_dependency{source_id, Sha256("source-product")};
    REQUIRE(dependencies.Replace(dependent_id, std::array{source_dependency}));
    AssetManager manager([](AssetId requested, task::CancellationToken) { return Ok(test::ProductFor(requested)); });
    ReloadCoordinator reload(vfs, database, dependencies, manager, 0ms);
    const std::array hints{ReloadHint{ReloadHintType::Changed, *uri}};
    reload.Push(hints);
    const auto invalidated = reload.Pump();
    REQUIRE(invalidated);
    REQUIRE(invalidated->size() == 2);
    REQUIRE(std::ranges::find(*invalidated, source_id) != invalidated->end());
    REQUIRE(std::ranges::find(*invalidated, dependent_id) != invalidated->end());
    REQUIRE(database.Find(source_id)->revision == 5);
    REQUIRE(manager.Status(source_id).version.revision == 1);
    REQUIRE(manager.Status(dependent_id).version.revision == 1);
}
