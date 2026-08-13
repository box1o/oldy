#include "../internal/runtime_state.hpp"

namespace woki::gfx {

Result<rhi::SubmissionTicket> RenderRuntime::Impl::PumpFacade() {
    const auto completed = device->GetQueue().CompletedSubmission();
    RuntimeFacadeAccess::PollCompute(*compute, completed);
    RuntimeFacadeAccess::PollReadbacks(*readback_service, completed);
    RuntimeFacadeGraph graph;
    auto appended = RuntimeFacadeAccess::AppendCompute(
        *compute,
        graph,
        *device,
        *assets.manager,
        layouts,
        pipeline_cache,
        completed
    );
    if (!appended) {
        RuntimeFacadeAccess::FailCompute(*compute, appended.error());
        return Err(std::move(appended).error());
    }
    appended = RuntimeFacadeAccess::AppendReadbacks(
        *readback_service,
        *compute,
        graph,
        *device,
        [this](const OffscreenTargetHandle handle) -> Result<ResolvedReadbackTexture> {
            if (!handle.IsValid() || handle.Index() >= offscreen_slots.size())
                return Err(ErrorCode::InvalidArgument, "readback references a stale offscreen target");
            const auto& slot = offscreen_slots[handle.Index()];
            if (slot.generation != handle.Generation() || !slot.descriptor || slot.texture == nullptr)
                return Err(ErrorCode::InvalidArgument, "readback references a stale offscreen target");
            GraphTextureDesc descriptor;
            descriptor.label = slot.descriptor->label;
            descriptor.extent = GraphExtent::Fixed(slot.descriptor->width, slot.descriptor->height);
            descriptor.sample_count = slot.descriptor->sample_count;
            descriptor.format = slot.texture->GetFormat();
            descriptor.usage = slot.texture->GetUsage();
            return Ok(ResolvedReadbackTexture{slot.texture, std::move(descriptor), slot.descriptor->format});
        }
    );
    if (!appended) {
        RuntimeFacadeAccess::FailCompute(*compute, appended.error());
        RuntimeFacadeAccess::FailReadbacks(*readback_service, appended.error());
        return Err(std::move(appended).error());
    }
    if (!graph.has_work)
        return Ok(rhi::SubmissionTicket{});
    auto compiled = graph.builder.Compile(graph.width, graph.height);
    if (!compiled) {
        RuntimeFacadeAccess::FailCompute(*compute, compiled.error());
        RuntimeFacadeAccess::FailReadbacks(*readback_service, compiled.error());
        return Err(std::move(compiled).error());
    }
    GraphExecutor executor(device, std::move(*compiled), resources->Releases());
    auto frame = executor.Begin(graph.width, graph.height);
    if (!frame) {
        RuntimeFacadeAccess::FailCompute(*compute, frame.error());
        RuntimeFacadeAccess::FailReadbacks(*readback_service, frame.error());
        return Err(std::move(frame).error());
    }
    auto bound = RuntimeFacadeAccess::BindCompute(*compute, *frame);
    if (!bound) {
        RuntimeFacadeAccess::FailCompute(*compute, bound.error());
        RuntimeFacadeAccess::FailReadbacks(*readback_service, bound.error());
        return Err(std::move(bound).error());
    }
    bound = RuntimeFacadeAccess::BindReadbacks(*readback_service, *frame);
    if (!bound) {
        RuntimeFacadeAccess::FailCompute(*compute, bound.error());
        RuntimeFacadeAccess::FailReadbacks(*readback_service, bound.error());
        return Err(std::move(bound).error());
    }
    auto submitted = frame->Execute();
    if (!submitted) {
        RuntimeFacadeAccess::FailCompute(*compute, submitted.error());
        RuntimeFacadeAccess::FailReadbacks(*readback_service, submitted.error());
        return Err(std::move(submitted).error());
    }
    RuntimeFacadeAccess::PublishCompute(*compute, layouts, pipeline_cache, *submitted);
    RuntimeFacadeAccess::PublishReadbacks(*readback_service, *submitted);
    return submitted;
}

ComputeService& RenderRuntime::Compute() noexcept {
    return *impl_->compute;
}

} // namespace woki::gfx
