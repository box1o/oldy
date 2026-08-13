#include <cstring>
#include <limits>

#include <woki/rhi/queue.hpp>
#include <woki/gfx/advanced/upload.hpp>
#include <woki/rhi/device.hpp>
#include <woki/rhi/command_encoder.hpp>

namespace woki::gfx {

UploadPublicationToken::UploadPublicationToken(ResidencyRecord initial, const u32 required_uploads, Completion completion, CompletionQueue completion_queue)
    : record_(std::move(initial)),
      expected_content_(record_.content_version),
      expected_residency_(record_.residency_version),
      remaining_(required_uploads),
      completion_(std::move(completion)),
      completion_queue_(std::move(completion_queue)) {}

std::shared_ptr<UploadPublicationToken> UploadPublicationToken::Create(ResidencyRecord initial, const u32 required_uploads, Completion completion, CompletionQueue completion_queue) {
    return std::shared_ptr<UploadPublicationToken>(new UploadPublicationToken(std::move(initial), required_uploads, std::move(completion), std::move(completion_queue)));
}

ResidencyRecord UploadPublicationToken::Snapshot() const noexcept {
    std::lock_guard lock(mutex_);
    return record_;
}

bool UploadPublicationToken::Expected() const noexcept {
    std::lock_guard lock(mutex_);
    return !terminal_ && record_.content_version == expected_content_ && record_.residency_version == expected_residency_;
}

void UploadPublicationToken::MarkPending() noexcept {
    std::lock_guard lock(mutex_);
    if (!terminal_)
        record_.residency = ResidencyState::UploadPending;
}

void UploadPublicationToken::Complete(const u64 bytes) {
    ResidencyRecord snapshot;
    bool notify{};
    {
        std::lock_guard lock(mutex_);
        if (terminal_ || record_.content_version != expected_content_ || record_.residency_version != expected_residency_)
            return;
        record_.estimated_bytes += bytes;
        if (remaining_ != 0)
            --remaining_;
        if (remaining_ == 0) {
            record_.residency = ResidencyState::Resident;
            static_cast<void>(record_.residency_version.Increment());
            terminal_ = true;
            snapshot = record_;
            notify = true;
        }
    }
    if (notify)
        Notify(true, snapshot, {});
}

void UploadPublicationToken::Fail(std::string diagnostic) {
    ResidencyRecord snapshot;
    {
        std::lock_guard lock(mutex_);
        if (terminal_)
            return;
        terminal_ = true;
        record_.resource = ResourceState::Failed;
        record_.residency = ResidencyState::NonResident;
        snapshot = record_;
    }
    Notify(false, snapshot, std::move(diagnostic));
}

void UploadPublicationToken::MarkDeviceLost() noexcept {
    ResidencyRecord snapshot;
    {
        std::lock_guard lock(mutex_);
        if (terminal_)
            return;
        terminal_ = true;
        MarkPhysicalResidencyLost(record_);
        snapshot = record_;
    }
    Notify(false, snapshot, "upload was abandoned because the device was lost");
}

void UploadPublicationToken::Notify(const bool success, ResidencyRecord snapshot, std::string diagnostic) {
    if (!completion_)
        return;
    auto invoke = [completion = completion_, success, snapshot, diagnostic = std::move(diagnostic)]() mutable { completion(success, snapshot, std::move(diagnostic)); };
    if (completion_queue_)
        completion_queue_(std::move(invoke));
    else
        invoke();
}

UploadScheduler::UploadScheduler(ref<rhi::Device> device, const UploadBudget budget)
    : device_(std::move(device)),
      budget_(budget) {}

Result<WorkVersion> UploadScheduler::Enqueue(BufferUploadRequest request) {
    return Enqueue(RequestData(std::move(request)));
}

Result<std::vector<WorkVersion>> UploadScheduler::EnqueueBatch(std::vector<BufferUploadRequest> requests) {
    if (requests.empty())
        return Err(ErrorCode::ValidationOutOfRange, "upload batch is empty");
    u64 bytes{};
    u64 in_flight_requests{};
    for (const auto& batch : in_flight_)
        in_flight_requests += batch.requests.size();
    for (const auto& request : requests) {
        const u64 size = request.bytes.size();
        if (size == 0 || size > budget_.batch_bytes || bytes > std::numeric_limits<u64>::max() - size || !Expected(RequestData(request)))
            return Err(ErrorCode::ValidationInvalidState, "upload batch contains an invalid or stale request");
        bytes += size;
    }
    const u64 retained_bytes = queued_bytes_ + in_flight_bytes_;
    const u64 retained_requests = queued_.size() + in_flight_requests;
    if (device_lost_ || device_ == nullptr)
        return Err(ErrorCode::GraphicsDeviceLost, "upload scheduler has no resident device");
    if (retained_requests + requests.size() > budget_.queued_requests || bytes > budget_.queued_bytes - std::min(retained_bytes, budget_.queued_bytes)) {
        ++rejected_;
        return Err(ErrorCode::QueueFull, "upload scheduler backpressure budget exceeded");
    }
    std::vector<WorkVersion> result;
    result.reserve(requests.size());
    for (auto& request : requests) {
        const WorkVersion work = next_work_;
        if (!next_work_.Increment())
            return Err(ErrorCode::QueueFull, "upload work version space exhausted");
        if (request.publication)
            request.publication->MarkPending();
        queued_bytes_ += request.bytes.size();
        queued_.push_back({work, RequestData(std::move(request))});
        result.push_back(work);
    }
    return Ok(std::move(result));
}

Result<WorkVersion> UploadScheduler::Enqueue(TextureUploadRequest request) {
    return Enqueue(RequestData(std::move(request)));
}

Result<WorkVersion> UploadScheduler::Enqueue(RequestData request) {
    const u64 bytes = ByteSize(request);
    if (device_lost_ || device_ == nullptr)
        return Err(ErrorCode::GraphicsDeviceLost, "upload scheduler has no resident device");
    if (bytes == 0)
        return Err(ErrorCode::ValidationOutOfRange, "upload request has no bytes");
    u64 in_flight_requests{};
    for (const auto& batch : in_flight_)
        in_flight_requests += batch.requests.size();
    const u64 retained_bytes = queued_bytes_ + in_flight_bytes_;
    const u64 retained_requests = queued_.size() + in_flight_requests;
    if (bytes > budget_.batch_bytes || retained_requests >= budget_.queued_requests || bytes > budget_.queued_bytes - std::min(retained_bytes, budget_.queued_bytes)) {
        ++rejected_;
        return Err(ErrorCode::QueueFull, "upload scheduler backpressure budget exceeded");
    }
    if (!Expected(request)) {
        ++stale_;
        return Err(ErrorCode::ValidationInvalidState, "upload request versions are stale");
    }
    const WorkVersion work = next_work_;
    if (!next_work_.Increment())
        return Err(ErrorCode::QueueFull, "upload work version space exhausted");
    if (auto publication = Publication(request))
        publication->MarkPending();
    queued_.push_back({work, std::move(request)});
    queued_bytes_ += bytes;
    return Ok(work);
}

Result<std::vector<UploadTicket>> UploadScheduler::PrepareAndSubmit() {
    if (device_lost_ || device_ == nullptr)
        return Err(ErrorCode::GraphicsDeviceLost, "upload scheduler has no resident device");
    std::erase_if(queued_, [&](const Request& request) {
        if (Expected(request.data))
            return false;
        queued_bytes_ -= ByteSize(request.data);
        ++stale_;
        return true;
    });

    size_t request_count{};
    u64 bytes{};
    while (request_count < queued_.size() && request_count < budget_.batch_requests) {
        const u64 request_bytes = ByteSize(queued_[request_count].data);
        if (request_count != 0 && request_bytes > budget_.batch_bytes - std::min(bytes, budget_.batch_bytes))
            break;
        bytes += request_bytes;
        ++request_count;
    }
    if (request_count == 0)
        return Ok(std::vector<UploadTicket>{});

    scope<rhi::CommandEncoder> encoder;
    TRY_ASSIGN(encoder, device_->CreateCommandEncoder({.label = "GfxUploads"}));
    std::vector<scope<rhi::Buffer>> staging;
    staging.reserve(request_count);
    for (size_t request_index = 0; request_index < request_count; ++request_index) {
        auto& request = queued_[request_index];
        const u64 size = ByteSize(request.data);
        const auto* texture_request = std::get_if<TextureUploadRequest>(&request.data);
        const u64 staging_size = texture_request == nullptr ? size : texture_request->layout.offset + size;
        if (staging_size < size)
            return Err(ErrorCode::ValidationOutOfRange, "texture upload staging range overflows");
        scope<rhi::Buffer> buffer;
        TRY_ASSIGN(buffer, device_->CreateBuffer({.size = staging_size, .usage = rhi::BufferUsage::MapWrite | rhi::BufferUsage::CopySrc, .mapped_at_creation = true, .label = "GfxUploadStaging"}));
        const auto copy_bytes = [&](const auto& upload, const u64 offset) -> Result<void> {
            TRY_VOID(buffer->WriteMappedRange(static_cast<size_t>(offset), upload.bytes.data(), upload.bytes.size()));
            buffer->Unmap();
            return Ok();
        };
        if (auto* upload = std::get_if<BufferUploadRequest>(&request.data)) {
            if (upload->target == nullptr || upload->offset > upload->target->GetSize() || size > upload->target->GetSize() - upload->offset)
                return Err(ErrorCode::ValidationOutOfRange, "buffer upload target range is invalid");
            TRY_VOID(copy_bytes(*upload, 0));
            TRY_VOID(encoder->CopyBufferToBuffer(*buffer, 0, *upload->target, upload->offset, size));
        } else {
            auto& texture_upload = std::get<TextureUploadRequest>(request.data);
            if (texture_upload.target == nullptr)
                return Err(ErrorCode::ValidationNullValue, "texture upload target is null");
            TRY_VOID(copy_bytes(texture_upload, texture_upload.layout.offset));
            const rhi::TexelCopyBufferInfo source{.layout = texture_upload.layout, .buffer = buffer.get()};
            const rhi::TexelCopyTextureInfo destination{.texture = texture_upload.target.get(), .mip_level = texture_upload.mip_level, .origin = texture_upload.origin, .aspect = texture_upload.aspect};
            TRY_VOID(encoder->CopyBufferToTexture(source, destination, texture_upload.extent));
        }
        staging.push_back(std::move(buffer));
    }
    scope<rhi::CommandBuffer> command;
    TRY_ASSIGN(command, encoder->Finish({.label = "GfxUploadsSubmit"}));
    rhi::CommandBuffer* commands[] = {command.get()};
    rhi::SubmissionTicket submission;
    TRY_ASSIGN(submission, device_->GetQueue().Submit(commands));
    std::vector<Request> requests;
    requests.reserve(request_count);
    for (size_t index = 0; index < request_count; ++index) {
        queued_bytes_ -= ByteSize(queued_.front().data);
        requests.push_back(std::move(queued_.front()));
        queued_.pop_front();
    }
    std::vector<UploadTicket> tickets;
    tickets.reserve(requests.size());
    for (const auto& request : requests)
        tickets.push_back({request.work, submission});
    in_flight_bytes_ += bytes;
    in_flight_.push_back({submission, std::move(requests), std::move(staging), bytes});
    return Ok(std::move(tickets));
}

size_t UploadScheduler::PublishCompleted(const rhi::SubmissionEpoch completed) {
    size_t count{};
    for (auto batch = in_flight_.begin(); batch != in_flight_.end();) {
        if (!completed.HasReached(batch->submission)) {
            ++batch;
            continue;
        }
        for (auto& request : batch->requests) {
            if (!Expected(request.data)) {
                ++stale_;
                continue;
            }
            if (auto publication = Publication(request.data))
                publication->Complete(ByteSize(request.data));
            ++published_;
            ++count;
        }
        in_flight_bytes_ -= batch->bytes;
        batch = in_flight_.erase(batch);
    }
    return count;
}

void UploadScheduler::MarkDeviceLost() noexcept {
    device_lost_ = true;
    for (auto& request : queued_) {
        if (auto publication = Publication(request.data))
            publication->MarkDeviceLost();
    }
    for (auto& batch : in_flight_) {
        for (auto& request : batch.requests) {
            if (auto publication = Publication(request.data))
                publication->MarkDeviceLost();
        }
    }
}

UploadStats UploadScheduler::Stats() const noexcept {
    u32 in_flight_requests{};
    for (const auto& batch : in_flight_)
        in_flight_requests += static_cast<u32>(batch.requests.size());
    return {.queued_bytes = queued_bytes_,
        .queued_requests = static_cast<u32>(queued_.size()),
        .in_flight_bytes = in_flight_bytes_,
        .in_flight_requests = in_flight_requests,
        .rejected_requests = rejected_,
        .stale_requests = stale_,
        .published_requests = published_};
}

u64 UploadScheduler::ByteSize(const RequestData& request) noexcept {
    return std::visit([](const auto& value) { return static_cast<u64>(value.bytes.size()); }, request);
}

std::shared_ptr<UploadPublicationToken> UploadScheduler::Publication(const RequestData& request) noexcept {
    return std::visit([](auto& value) { return value.publication; }, request);
}

bool UploadScheduler::Expected(const RequestData& request) noexcept {
    return std::visit([](const auto& value) { return !value.publication || value.publication->Expected(); }, request);
}

} // namespace woki::gfx
