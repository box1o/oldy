#pragma once

#include <atomic>
#include <deque>

#include <woki/rhi/objects.hpp>

#include <woki/gfx/advanced/epoch.hpp>
#include <woki/gfx/readback.hpp>

namespace woki::gfx {

struct ReadbackRequest final {
    ref<rhi::Buffer> source;
    u64 offset{};
    u64 size{};
};

struct ReadbackSubmission final {
    ReadbackTicket ticket;
    rhi::SubmissionTicket submission;
};

class ReadbackManager final {
public:
    explicit ReadbackManager(ref<rhi::Device> device, u64 byte_budget = 32ull * 1024ull * 1024ull, u32 request_budget = 256);
    [[nodiscard]] Result<ReadbackTicket> Enqueue(ReadbackRequest request);
    [[nodiscard]] Result<std::vector<ReadbackSubmission>> PrepareAndSubmit();
    size_t Poll(rhi::SubmissionEpoch completed);
    [[nodiscard]] ReadbackState State(ReadbackTicket ticket) const noexcept;
    [[nodiscard]] Result<std::vector<std::byte>> Take(ReadbackTicket ticket);
    void MarkDeviceLost() noexcept;

private:
    struct Entry final {
        ReadbackTicket ticket;
        ReadbackRequest request;
        scope<rhi::Buffer> staging;
        rhi::SubmissionTicket submission;
        std::vector<std::byte> bytes;
        std::atomic<ReadbackState> state{ReadbackState::Queued};
    };

    [[nodiscard]] std::shared_ptr<Entry> Find(ReadbackTicket ticket) const noexcept;
    ref<rhi::Device> device_;
    u64 byte_budget_{};
    u32 request_budget_{};
    u64 retained_bytes_{};
    u64 next_ticket_{1};
    bool device_lost_{};
    std::deque<std::shared_ptr<Entry>> queued_;
    std::vector<std::shared_ptr<Entry>> entries_;
};

} // namespace woki::gfx
