#include <woki/rhi/queue.hpp>
#include <woki/gfx/advanced/runtime_services.hpp>

#include "internal/readback_manager.hpp"

namespace woki::gfx {

Result<scope<RenderRuntimeServices>> RenderRuntimeServices::Create(ref<rhi::Device> device, RenderRuntimeDesc descriptor) {
    if (device == nullptr || descriptor.frames_in_flight == 0)
        return Err(ErrorCode::ValidationInvalidState, "render runtime descriptor is invalid");
    auto services = scope<RenderRuntimeServices>(new RenderRuntimeServices(std::move(device), descriptor));
    for (auto& pool_descriptor : descriptor.buffer_pools) {
        scope<BufferPool> pool;
        TRY_ASSIGN(pool, BufferPool::Create(services->device_, std::move(pool_descriptor), services->releases_));
        if (services->Pool(pool->Descriptor().pool_class) != nullptr)
            return Err(ErrorCode::ValidationInvalidState, "render runtime buffer pool class is duplicated");
        services->pools_.push_back(std::move(pool));
    }
    return Ok(std::move(services));
}

RenderRuntimeServices::RenderRuntimeServices(ref<rhi::Device> device, const RenderRuntimeDesc& descriptor)
    : device_(std::move(device)),
      frames_(descriptor.frames_in_flight, descriptor.frame_scratch_bytes),
      uploads_(device_, descriptor.upload_budget),
      readbacks_(createScope<ReadbackManager>(device_, descriptor.readback_budget_bytes, descriptor.readback_request_budget)),
      releases_(createRef<DeferredReleaseQueue>()) {}

RenderRuntimeServices::~RenderRuntimeServices() = default;

Result<std::reference_wrapper<FrameContext>> RenderRuntimeServices::AcquireFrame() {
    if (device_lost_)
        return Err(ErrorCode::GraphicsDeviceLost, "render runtime device is lost");
    Collect();
    return frames_.Acquire(device_->GetQueue());
}

void RenderRuntimeServices::Collect() {
    if (device_ == nullptr)
        return;
    const auto completed = device_->GetQueue().CompletedSubmission();
    static_cast<void>(releases_->Collect(completed));
    static_cast<void>(uploads_.PublishCompleted(completed));
    static_cast<void>(readbacks_->Poll(completed));
    for (auto& pool : pools_)
        static_cast<void>(pool->Collect(completed));
}

void RenderRuntimeServices::MarkDeviceLost() noexcept {
    device_lost_ = true;
    uploads_.MarkDeviceLost();
    readbacks_->MarkDeviceLost();
    for (auto& pool : pools_)
        pool->MarkResidencyLost();
}

BufferPool* RenderRuntimeServices::Pool(const BufferPoolClass pool_class) noexcept {
    const auto found = std::ranges::find_if(pools_, [&](const auto& pool) { return pool->Descriptor().pool_class == pool_class; });
    return found == pools_.end() ? nullptr : found->get();
}

RenderRuntimeDiagnostics RenderRuntimeServices::Diagnostics() const {
    RenderRuntimeDiagnostics result{.next_frame = frames_.NextEpoch(), .uploads = uploads_.Stats(), .pools = {}, .deferred_releases = releases_->PendingCount(), .device_lost = device_lost_};
    result.pools.reserve(pools_.size());
    for (const auto& pool : pools_)
        result.pools.push_back(pool->Stats());
    return result;
}

} // namespace woki::gfx
