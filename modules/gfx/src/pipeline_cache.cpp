#include <woki/gfx/advanced/pipeline_cache.hpp>

namespace woki::gfx {
namespace {
template <typename Key>
Result<void> Normalize(Key& key) {
    std::ranges::sort(key.overrides, {}, &PipelineOverride::id);
    if (std::ranges::adjacent_find(key.overrides, {}, &PipelineOverride::id) != key.overrides.end())
        return Err(ErrorCode::InvalidArgument, "pipeline key contains a duplicate override ID");
    if (key.shader_product == ContentHash{} || key.shader_variant == ContentHash{} || key.pipeline_layout == ContentHash{} || key.shader_generation == 0)
        return Err(ErrorCode::InvalidArgument, "pipeline key identity is incomplete");
    return Ok();
}

Result<void> Validate(const GraphicsPipelineKey& key) {
    const bool depth_only = key.fragment_entry.Empty() && key.targets.colors.empty() && key.targets.depth != rhi::TextureFormat::Undefined;
    const bool color = !key.fragment_entry.Empty() && !key.targets.colors.empty();
    if (key.vertex_entry.Empty() || key.vertex_schema_id == 0 || key.render_state == ContentHash{} || key.targets.samples == 0 || (!depth_only && !color))
        return Err(ErrorCode::InvalidArgument, "graphics pipeline key identity is incomplete");
    return Ok();
}

Result<void> Validate(const ComputePipelineKey& key) {
    return key.compute_entry.Empty() ? Result<void>(Err(ErrorCode::InvalidArgument, "compute pipeline key has no entry point")) : Ok();
}
} // namespace

Result<PipelineRequest<rhi::RenderPipeline>> PipelineCache::Request(GraphicsPipelineKey key, GraphicsFactory create) {
    if (owner_ != std::this_thread::get_id())
        return Err(ErrorCode::InvalidState, "pipeline cache is owner-thread only");
    TRY_VOID(Normalize(key));
    TRY_VOID(Validate(key));
    if (const auto found = graphics_.find(key); found != graphics_.end())
        return Ok(found->second.request);
    PipelineRequest<rhi::RenderPipeline> request;
    auto created = create();
    if (created) {
        request.state = PipelineRequestState::Ready;
        request.pipeline = std::move(*created);
    } else {
        request.state = PipelineRequestState::Failed;
        request.diagnostic = std::string(created.error().Message());
    }
    graphics_.emplace(std::move(key), Entry<rhi::RenderPipeline>{request, {}});
    return Ok(std::move(request));
}

Result<PipelineRequest<rhi::ComputePipeline>> PipelineCache::Request(ComputePipelineKey key, ComputeFactory create) {
    if (owner_ != std::this_thread::get_id())
        return Err(ErrorCode::InvalidState, "pipeline cache is owner-thread only");
    TRY_VOID(Normalize(key));
    TRY_VOID(Validate(key));
    if (const auto found = compute_.find(key); found != compute_.end())
        return Ok(found->second.request);
    PipelineRequest<rhi::ComputePipeline> request;
    auto created = create();
    if (created) {
        request.state = PipelineRequestState::Ready;
        request.pipeline = std::move(*created);
    } else {
        request.state = PipelineRequestState::Failed;
        request.diagnostic = std::string(created.error().Message());
    }
    compute_.emplace(std::move(key), Entry<rhi::ComputePipeline>{request, {}});
    return Ok(std::move(request));
}

PipelineRequest<rhi::RenderPipeline> PipelineCache::Find(const GraphicsPipelineKey& key) const {
    const auto found = graphics_.find(key);
    return found == graphics_.end() ? PipelineRequest<rhi::RenderPipeline>{} : found->second.request;
}

PipelineRequest<rhi::ComputePipeline> PipelineCache::Find(const ComputePipelineKey& key) const {
    const auto found = compute_.find(key);
    return found == compute_.end() ? PipelineRequest<rhi::ComputePipeline>{} : found->second.request;
}

Result<void> PipelineCache::MarkUsed(GraphicsPipelineKey key, const rhi::SubmissionTicket submission) {
    if (owner_ != std::this_thread::get_id())
        return Err(ErrorCode::InvalidState, "pipeline cache is owner-thread only");
    TRY_VOID(Normalize(key));
    TRY_VOID(Validate(key));
    if (const auto found = graphics_.find(key); found != graphics_.end() && found->second.last_used < submission)
        found->second.last_used = submission;
    return Ok();
}

Result<void> PipelineCache::MarkUsed(ComputePipelineKey key, const rhi::SubmissionTicket submission) {
    if (owner_ != std::this_thread::get_id())
        return Err(ErrorCode::InvalidState, "pipeline cache is owner-thread only");
    TRY_VOID(Normalize(key));
    TRY_VOID(Validate(key));
    if (const auto found = compute_.find(key); found != compute_.end() && found->second.last_used < submission)
        found->second.last_used = submission;
    return Ok();
}

Result<size_t> PipelineCache::InvalidateShader(const ContentHash shader, DeferredReleaseQueue& releases) {
    if (owner_ != std::this_thread::get_id())
        return Err(ErrorCode::InvalidState, "pipeline cache is owner-thread only");
    const size_t before = graphics_.size() + compute_.size();
    std::erase_if(graphics_, [&](auto& item) {
        if (item.first.shader_product != shader)
            return false;
        if (item.second.request.pipeline && item.second.last_used.IsValid())
            releases.Retire(std::move(item.second.request.pipeline), item.second.last_used);
        return true;
    });
    std::erase_if(compute_, [&](auto& item) {
        if (item.first.shader_product != shader)
            return false;
        if (item.second.request.pipeline && item.second.last_used.IsValid())
            releases.Retire(std::move(item.second.request.pipeline), item.second.last_used);
        return true;
    });
    return Ok(before - graphics_.size() - compute_.size());
}

} // namespace woki::gfx
