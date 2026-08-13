#include <woki/gfx/advanced/epoch.hpp>
#include <woki/rhi/queue.hpp>

namespace woki::gfx {

FrameContext::FrameContext(const u64 scratch_bytes)
    : scratch_(scratch_bytes) {}

Result<void> FrameContext::Begin(const FrameEpoch epoch) {
    if (state_ != FrameState::Available || !epoch.IsValid())
        return Err(ErrorCode::InvalidState, "frame context is not available");
    scratch_.clear();
    epoch_ = epoch;
    submission_ = {};
    state_ = FrameState::Recording;
    return Ok();
}

Result<void> FrameContext::Submit(const rhi::SubmissionTicket submission) {
    if (state_ != FrameState::Recording || !submission.IsValid())
        return Err(ErrorCode::InvalidState, "frame context requires a valid submission");
    submission_ = submission;
    state_ = FrameState::Submitted;
    return Ok();
}

Result<void> FrameContext::Cancel() {
    if (state_ != FrameState::Recording)
        return Err(ErrorCode::InvalidState, "only a recording frame context can be cancelled");
    submission_ = {};
    state_ = FrameState::Available;
    return Ok();
}

Result<void> FrameContext::Retire(const rhi::SubmissionEpoch completed) {
    if (state_ != FrameState::Submitted || !completed.HasReached(submission_))
        return Err(ErrorCode::InvalidState, "frame context submission has not completed");
    submission_ = {};
    state_ = FrameState::Available;
    return Ok();
}

FrameContextRing::FrameContextRing(const u32 frames_in_flight, const u64 scratch_bytes_per_frame) {
    contexts_.reserve(frames_in_flight);
    for (u32 index = 0; index < frames_in_flight; ++index)
        contexts_.push_back(createScope<FrameContext>(scratch_bytes_per_frame));
}

Result<std::reference_wrapper<FrameContext>> FrameContextRing::Acquire(const rhi::SubmissionEpoch completed) {
    if (contexts_.empty())
        return Err(ErrorCode::InvalidState, "frame context ring has zero capacity");
    for (size_t attempt = 0; attempt < contexts_.size(); ++attempt) {
        const size_t index = (cursor_ + attempt) % contexts_.size();
        auto& context = *contexts_[index];
        if (context.State() == FrameState::Submitted && completed.HasReached(context.Submission()))
            TRY_VOID(context.Retire(completed));
        if (context.State() != FrameState::Available)
            continue;
        const FrameEpoch epoch = next_epoch_;
        if (!next_epoch_.Increment())
            return Err(ErrorCode::FailedToAcquireResource, "frame epoch space exhausted");
        TRY_VOID(context.Begin(epoch));
        cursor_ = (index + 1) % contexts_.size();
        return Ok(std::ref(context));
    }
    return Err(ErrorCode::FailedToAcquireResource, "all frame contexts are in flight");
}

Result<std::reference_wrapper<FrameContext>> FrameContextRing::Acquire(const rhi::Queue& queue) {
    return Acquire(queue.CompletedSubmission());
}

u32 FrameContextRing::Capacity() const noexcept {
    return static_cast<u32>(contexts_.size());
}

FrameEpoch FrameContextRing::NextEpoch() const noexcept {
    return next_epoch_;
}

} // namespace woki::gfx
