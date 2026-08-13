#include <woki/rhi/queue.hpp>
#include <woki/rhi/device.hpp>
#include "internal/readback_manager.hpp"
#include <woki/rhi/command_encoder.hpp>

namespace woki::gfx {

ReadbackManager::ReadbackManager(ref<rhi::Device> device, const u64 byte_budget, const u32 request_budget)
    : device_(std::move(device)),
      byte_budget_(byte_budget),
      request_budget_(request_budget) {}

Result<ReadbackTicket> ReadbackManager::Enqueue(ReadbackRequest request) {
    if (device_lost_ || device_ == nullptr)
        return Err(ErrorCode::GraphicsDeviceLost, "readback manager has no resident device");
    if (request.source == nullptr || request.size == 0 || request.offset > request.source->GetSize() || request.size > request.source->GetSize() - request.offset)
        return Err(ErrorCode::ValidationOutOfRange, "readback source range is invalid");
    if (entries_.size() >= request_budget_ || request.size > byte_budget_ - std::min(retained_bytes_, byte_budget_))
        return Err(ErrorCode::QueueFull, "readback budget exceeded");
    auto entry = std::make_shared<Entry>();
    entry->ticket = ReadbackTicket(next_ticket_++);
    entry->request = std::move(request);
    retained_bytes_ += entry->request.size;
    queued_.push_back(entry);
    entries_.push_back(entry);
    return Ok(entry->ticket);
}

Result<std::vector<ReadbackSubmission>> ReadbackManager::PrepareAndSubmit() {
    if (device_lost_ || device_ == nullptr)
        return Err(ErrorCode::GraphicsDeviceLost, "readback manager has no resident device");
    if (queued_.empty())
        return Ok(std::vector<ReadbackSubmission>{});
    scope<rhi::CommandEncoder> encoder;
    TRY_ASSIGN(encoder, device_->CreateCommandEncoder({.label = "GfxReadbacks"}));
    std::vector<std::shared_ptr<Entry>> submitted;
    while (!queued_.empty()) {
        auto entry = queued_.front();
        queued_.pop_front();
        TRY_ASSIGN(entry->staging, device_->CreateBuffer({.size = entry->request.size, .usage = rhi::BufferUsage::MapRead | rhi::BufferUsage::CopyDst, .label = "GfxReadbackStaging"}));
        TRY_VOID(encoder->CopyBufferToBuffer(*entry->request.source, entry->request.offset, *entry->staging, 0, entry->request.size));
        submitted.push_back(std::move(entry));
    }
    scope<rhi::CommandBuffer> command;
    TRY_ASSIGN(command, encoder->Finish({.label = "GfxReadbacksSubmit"}));
    rhi::CommandBuffer* commands[] = {command.get()};
    rhi::SubmissionTicket submission;
    TRY_ASSIGN(submission, device_->GetQueue().Submit(commands));
    std::vector<ReadbackSubmission> result;
    result.reserve(submitted.size());
    for (auto& entry : submitted) {
        entry->submission = submission;
        entry->state.store(ReadbackState::Submitted, std::memory_order_release);
        result.push_back({entry->ticket, submission});
    }
    return Ok(std::move(result));
}

size_t ReadbackManager::Poll(const rhi::SubmissionEpoch completed) {
    size_t started{};
    for (const auto& entry : entries_) {
        ReadbackState expected = ReadbackState::Submitted;
        if (!completed.HasReached(entry->submission) || !entry->state.compare_exchange_strong(expected, ReadbackState::Mapping, std::memory_order_acq_rel))
            continue;
        ++started;
        const auto retained = entry;
        static_cast<void>(entry->staging
                ->MapAsync(rhi::MapMode::Read, 0, static_cast<size_t>(entry->request.size), rhi::CallbackMode::AllowSpontaneous, [retained](const rhi::MapAsyncStatus status, const std::string_view) {
                    if (status != rhi::MapAsyncStatus::Success) {
                        retained->state.store(ReadbackState::Failed, std::memory_order_release);
                        return;
                    }
                    retained->bytes.resize(static_cast<size_t>(retained->request.size));
                    const auto read = retained->staging->ReadMappedRange(0, retained->bytes.data(), retained->bytes.size());
                    retained->staging->Unmap();
                    retained->state.store(read ? ReadbackState::Ready : ReadbackState::Failed, std::memory_order_release);
                }));
    }
    return started;
}

ReadbackState ReadbackManager::State(const ReadbackTicket ticket) const noexcept {
    const auto entry = Find(ticket);
    return entry == nullptr ? ReadbackState::Failed : entry->state.load(std::memory_order_acquire);
}

Result<std::vector<std::byte>> ReadbackManager::Take(const ReadbackTicket ticket) {
    const auto entry = Find(ticket);
    if (entry == nullptr)
        return Err(ErrorCode::ValidationInvalidState, "readback ticket is stale");
    ReadbackState expected = ReadbackState::Ready;
    if (!entry->state.compare_exchange_strong(expected, ReadbackState::Consumed, std::memory_order_acq_rel))
        return Err(ErrorCode::InvalidState, "readback is not ready");
    retained_bytes_ -= entry->request.size;
    auto result = std::move(entry->bytes);
    std::erase(entries_, entry);
    return Ok(std::move(result));
}

void ReadbackManager::MarkDeviceLost() noexcept {
    device_lost_ = true;
    for (const auto& entry : entries_) {
        const auto state = entry->state.load(std::memory_order_acquire);
        if (state != ReadbackState::Ready && state != ReadbackState::Consumed)
            entry->state.store(ReadbackState::DeviceLost, std::memory_order_release);
    }
}

std::shared_ptr<ReadbackManager::Entry> ReadbackManager::Find(const ReadbackTicket ticket) const noexcept {
    const auto found = std::ranges::find_if(entries_, [&](const auto& entry) { return entry->ticket == ticket; });
    return found == entries_.end() ? nullptr : *found;
}

} // namespace woki::gfx
