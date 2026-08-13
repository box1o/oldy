#pragma once

#include <functional>

#include <woki/core.hpp>
#include <woki/rhi/submission.hpp>

namespace woki::rhi {
class Queue;
}

namespace woki::gfx {

struct FrameEpochTag;
struct WorkVersionTag;
struct ContentVersionTag;
struct ResidencyVersionTag;
struct AllocationIdTag;

using FrameEpoch = Version<FrameEpochTag>;
using WorkVersion = Version<WorkVersionTag>;
using ContentVersion = Version<ContentVersionTag>;
using ResidencyVersion = Version<ResidencyVersionTag>;
using AllocationId = Version<AllocationIdTag>;

enum class FrameState : u8 {
    Available,
    Recording,
    Submitted,
};

class FrameContext final {
public:
    explicit FrameContext(u64 scratch_bytes = 8ull * 1024ull * 1024ull);

    FrameContext(const FrameContext&) = delete;
    FrameContext& operator=(const FrameContext&) = delete;
    FrameContext(FrameContext&&) = delete;
    FrameContext& operator=(FrameContext&&) = delete;

    [[nodiscard]] Result<void> Begin(FrameEpoch epoch);
    [[nodiscard]] Result<void> Submit(rhi::SubmissionTicket submission);
    [[nodiscard]] Result<void> Cancel();
    [[nodiscard]] Result<void> Retire(rhi::SubmissionEpoch completed);

    [[nodiscard]] Arena& Scratch() noexcept {
        return scratch_;
    }

    [[nodiscard]] const Arena& Scratch() const noexcept {
        return scratch_;
    }

    [[nodiscard]] FrameEpoch Epoch() const noexcept {
        return epoch_;
    }

    [[nodiscard]] rhi::SubmissionTicket Submission() const noexcept {
        return submission_;
    }

    [[nodiscard]] FrameState State() const noexcept {
        return state_;
    }

private:
    Arena scratch_;
    FrameEpoch epoch_;
    rhi::SubmissionTicket submission_;
    FrameState state_{FrameState::Available};
};

class FrameContextRing final {
public:
    explicit FrameContextRing(u32 frames_in_flight = 3, u64 scratch_bytes_per_frame = 8ull * 1024ull * 1024ull);

    [[nodiscard]] Result<std::reference_wrapper<FrameContext>> Acquire(rhi::SubmissionEpoch completed);
    [[nodiscard]] Result<std::reference_wrapper<FrameContext>> Acquire(const rhi::Queue& queue);
    [[nodiscard]] u32 Capacity() const noexcept;
    [[nodiscard]] FrameEpoch NextEpoch() const noexcept;

private:
    std::vector<scope<FrameContext>> contexts_;
    size_t cursor_{};
    FrameEpoch next_epoch_{1};
};

} // namespace woki::gfx
