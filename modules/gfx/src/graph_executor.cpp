#include <woki/rhi/queue.hpp>
#include <woki/rhi/device.hpp>
#include <woki/rhi/objects.hpp>
#include <woki/gfx/advanced/graph_executor.hpp>
#include <woki/rhi/command_encoder.hpp>
#include <woki/rhi/render_pass_encoder.hpp>
#include <woki/rhi/compute_pass_encoder.hpp>

#include "internal/graph.hpp"

namespace woki::gfx {
namespace {

constexpr size_t kMaxGraphFramesInFlight = 3;

[[nodiscard]] bool SameTextureSignature(
    const GraphTextureDesc& expected,
    const GraphTextureDesc& actual,
    const rhi::Extent3D& extent,
    const u32 frame_width,
    const u32 frame_height
) {
    const auto bound_extent = graph_detail::ResolveExtent(actual, frame_width, frame_height);
    return expected.format == actual.format && expected.sample_count == actual.sample_count
           && expected.mip_levels == actual.mip_levels && expected.depth_or_layers == actual.depth_or_layers
           && expected.dimension == actual.dimension && bound_extent.width == extent.width
           && bound_extent.height == extent.height && bound_extent.depth_or_array_layers == extent.depth_or_array_layers
           && expected.view_formats == actual.view_formats && expected.view_aspect == actual.view_aspect
           && (static_cast<u64>(actual.usage) & static_cast<u64>(expected.usage)) == static_cast<u64>(expected.usage);
}

[[nodiscard]] bool SamePhysicalTexture(
    const graph_detail::Resource& resource,
    const rhi::Texture& texture,
    const rhi::Extent3D& extent
) {
    return texture.GetFormat() == resource.texture.format && texture.GetSampleCount() == resource.texture.sample_count
           && texture.GetMipLevelCount() == resource.texture.mip_levels
           && texture.GetDepthOrArrayLayers() == extent.depth_or_array_layers
           && texture.GetDimension() == resource.texture.dimension && texture.GetWidth() == extent.width
           && texture.GetHeight() == extent.height
           && (static_cast<u64>(texture.GetUsage()) & static_cast<u64>(resource.texture.usage))
                  == static_cast<u64>(resource.texture.usage);
}

[[nodiscard]] rhi::TexelCopyTextureInfo CopyInfo(rhi::Texture& texture) {
    return {.texture = &texture, .mip_level = 0, .origin = {}, .aspect = rhi::TextureAspect::All};
}

[[nodiscard]] rhi::TextureViewDimension DefaultViewDimension(
    const GraphTextureDesc& descriptor,
    const TextureSubresourceRange range
) {
    if (descriptor.dimension == rhi::TextureDimension::e3D)
        return rhi::TextureViewDimension::e3D;
    return range.array_layer_count > 1 ? rhi::TextureViewDimension::e2DArray : rhi::TextureViewDimension::e2D;
}

} // namespace

struct RenderGraphContext::Runtime final {
    struct CachedView final {
        u32 resource{};
        GraphTextureViewDesc descriptor;
        scope<rhi::TextureView> view;
    };

    rhi::Device* device{};
    rhi::CommandEncoder* command{};
    rhi::RenderPassEncoder* render{};
    rhi::ComputePassEncoder* compute{};
    graph_detail::Definition* definition{};
    const std::vector<rhi::Extent3D>* extents{};
    std::vector<rhi::Texture*> textures;
    std::vector<rhi::TextureView*> views;
    std::vector<rhi::Buffer*> buffers;
    std::vector<CachedView> cached_views;
};

namespace {

Result<std::reference_wrapper<rhi::TextureView>> ResolveView(
    RenderGraphContext::Runtime& runtime,
    const u32 logical,
    GraphTextureViewDesc view
) {
    const auto& resource = runtime.definition->resources[logical];
    view.subresources = graph_detail::NormalizeRange(resource.texture, view.subresources);
    if (view.format == rhi::TextureFormat::Undefined)
        view.format = resource.texture.format;
    if (view.dimension == rhi::TextureViewDimension::Undefined)
        view.dimension = DefaultViewDimension(resource.texture, view.subresources);
    if (view.usage == rhi::TextureUsage::None)
        view.usage = resource.texture.usage;
    const auto found = std::ranges::find_if(runtime.cached_views, [&](const auto& cached) {
        return cached.resource == logical && cached.descriptor == view;
    });
    if (found != runtime.cached_views.end())
        return Ok(std::ref(*found->view));
    if (runtime.textures[logical] == nullptr) {
        if (runtime.views[logical] != nullptr && view.subresources.base_mip_level == 0
            && view.subresources.mip_level_count == resource.texture.mip_levels
            && view.subresources.base_array_layer == 0
            && view.subresources.array_layer_count == resource.texture.depth_or_layers
            && view.format == resource.texture.format)
            return Ok(std::ref(*runtime.views[logical]));
        return Err(ErrorCode::GraphicsResourceCreationFailed, "GRF2006 subresource view requires a texture binding");
    }
    auto created = runtime.textures[logical]->CreateView(
        {.format = view.format,
            .dimension = view.dimension,
            .base_mip_level = view.subresources.base_mip_level,
            .mip_level_count = view.subresources.mip_level_count,
            .base_array_layer = view.subresources.base_array_layer,
            .array_layer_count = view.subresources.array_layer_count,
            .aspect = view.subresources.aspect,
            .usage = view.usage,
            .label = view.label.empty() ? resource.texture.label + ".SubresourceView" : view.label}
    );
    if (created == nullptr)
        return Err(ErrorCode::GraphicsResourceCreationFailed, "GRF2003 failed to create texture subresource view");
    runtime.cached_views.push_back({logical, std::move(view), std::move(created)});
    return Ok(std::ref(*runtime.cached_views.back().view));
}

} // namespace

struct graph_detail::ExecutorState final {
    struct PhysicalSet {
        std::vector<scope<rhi::Texture>> textures;
        std::vector<scope<rhi::TextureView>> views;
        std::vector<scope<rhi::Buffer>> buffers;
    };

    struct RetiredSet {
        rhi::SubmissionTicket submission;
        PhysicalSet physical;
    };

    ref<rhi::Device> device;
    std::shared_ptr<GraphCommandBackend> backend;
    ref<DeferredReleaseQueue> releases;
    CompiledRenderGraph graph;
    bool valid{true};
    bool active{};
    u32 width{};
    u32 height{};
    scope<rhi::CommandEncoder> encoder;
    PhysicalSet physical;
    std::vector<PhysicalSet> available;
    std::vector<RetiredSet> retired;
    size_t physical_set_count{};
    bool physical_acquired{};
    rhi::SubmissionTicket last_submission;
    std::vector<ref<rhi::Texture>> bound_textures;
    std::vector<ref<rhi::TextureView>> bound_views;
    std::vector<ref<rhi::Buffer>> bound_buffers;
    std::vector<ref<rhi::Texture>> temporal_textures;
    std::vector<ref<rhi::TextureView>> temporal_views;
    std::vector<ref<rhi::TextureView>> static_views;
    std::vector<std::pair<u32, u32>> temporal_pairs;
};

rhi::Device& RenderGraphContext::Device() const noexcept {
    return *runtime_->device;
}

rhi::CommandEncoder& RenderGraphContext::CommandEncoder() const noexcept {
    return *runtime_->command;
}

rhi::RenderPassEncoder* RenderGraphContext::RenderEncoder() const noexcept {
    return runtime_->render;
}

rhi::ComputePassEncoder* RenderGraphContext::ComputeEncoder() const noexcept {
    return runtime_->compute;
}

GraphBlackboard& RenderGraphContext::Blackboard() const noexcept {
    return runtime_->definition->blackboard;
}

Result<std::reference_wrapper<rhi::Texture>> RenderGraphContext::Texture(const GraphTextureRef resource) const {
    if (!graph_detail::Belongs(*runtime_->definition, resource)
        || resource.Index() >= runtime_->definition->versions.size())
        return Err(ErrorCode::ValidationInvalidState, "GRF2004 stale or cross-graph texture access");
    const auto& pass = runtime_->definition->passes[pass_];
    if (std::ranges::none_of(pass.uses, [&](const auto& use) { return use.version == resource.Index(); }))
        return Err(ErrorCode::ValidationInvalidState, "GRF2005 execute callback accessed an undeclared texture");
    const u32 logical = runtime_->definition->versions[resource.Index()].resource;
    if (logical >= runtime_->textures.size() || runtime_->textures[logical] == nullptr)
        return Err(ErrorCode::GraphicsResourceCreationFailed, "GRF2006 texture has no physical binding");
    return Ok(std::ref(*runtime_->textures[logical]));
}

Result<std::reference_wrapper<rhi::TextureView>> RenderGraphContext::TextureView(const GraphTextureRef resource) const {
    if (!graph_detail::Belongs(*runtime_->definition, resource)
        || resource.Index() >= runtime_->definition->versions.size())
        return Err(ErrorCode::ValidationInvalidState, "GRF2004 stale or cross-graph texture view access");
    const auto& pass = runtime_->definition->passes[pass_];
    if (std::ranges::none_of(pass.uses, [&](const auto& use) { return use.version == resource.Index(); }))
        return Err(ErrorCode::ValidationInvalidState, "GRF2005 execute callback accessed an undeclared texture view");
    const auto declared = std::ranges::find_if(pass.uses, [&](const auto& use) {
        return use.version == resource.Index();
    });
    const u32 logical = runtime_->definition->versions[resource.Index()].resource;
    return ResolveView(*runtime_, logical, declared->view);
}

Result<std::reference_wrapper<rhi::TextureView>> RenderGraphContext::TextureView(
    const GraphTextureRef resource,
    GraphTextureViewDesc view
) const {
    if (!graph_detail::Belongs(*runtime_->definition, resource)
        || resource.Index() >= runtime_->definition->versions.size())
        return Err(ErrorCode::ValidationInvalidState, "GRF2004 stale or cross-graph texture view access");
    const auto& pass = runtime_->definition->passes[pass_];
    const auto declared = std::ranges::find_if(pass.uses, [&](const auto& use) {
        return use.version == resource.Index() && (use.view == view || use.subresources == view.subresources);
    });
    if (declared == pass.uses.end())
        return Err(
            ErrorCode::ValidationInvalidState,
            "GRF2005 execute callback accessed an undeclared texture subresource view"
        );
    return ResolveView(*runtime_, runtime_->definition->versions[resource.Index()].resource, std::move(view));
}

Result<std::reference_wrapper<rhi::Buffer>> RenderGraphContext::Buffer(const GraphBufferRef resource) const {
    if (!graph_detail::Belongs(*runtime_->definition, resource)
        || resource.Index() >= runtime_->definition->versions.size())
        return Err(ErrorCode::ValidationInvalidState, "GRF2004 stale or cross-graph buffer access");
    const auto& pass = runtime_->definition->passes[pass_];
    if (std::ranges::none_of(pass.uses, [&](const auto& use) { return use.version == resource.Index(); }))
        return Err(ErrorCode::ValidationInvalidState, "GRF2005 execute callback accessed an undeclared buffer");
    const u32 logical = runtime_->definition->versions[resource.Index()].resource;
    if (logical >= runtime_->buffers.size() || runtime_->buffers[logical] == nullptr)
        return Err(ErrorCode::GraphicsResourceCreationFailed, "GRF2006 buffer has no physical binding");
    return Ok(std::ref(*runtime_->buffers[logical]));
}

rhi::Extent3D RenderGraphContext::Extent(const GraphTextureRef resource) const noexcept {
    if (!graph_detail::Belongs(*runtime_->definition, resource)
        || resource.Index() >= runtime_->definition->versions.size())
        return {};
    return (*runtime_->extents)[runtime_->definition->versions[resource.Index()].resource];
}

rhi::Extent3D RenderGraphContext::Extent(const GraphTextureRef resource, TextureSubresourceRange range) const noexcept {
    if (!graph_detail::Belongs(*runtime_->definition, resource)
        || resource.Index() >= runtime_->definition->versions.size())
        return {};
    const u32 logical = runtime_->definition->versions[resource.Index()].resource;
    range = graph_detail::NormalizeRange(runtime_->definition->resources[logical].texture, range);
    return graph_detail::SubresourceExtent((*runtime_->extents)[logical], range);
}

GraphExecutor::GraphExecutor(ref<rhi::Device> device, CompiledRenderGraph graph, ref<DeferredReleaseQueue> releases)
    : state_(std::make_shared<graph_detail::ExecutorState>()) {
    state_->device = std::move(device);
    state_->releases = releases ? std::move(releases) : createRef<DeferredReleaseQueue>();
    state_->graph = std::move(graph);
    const size_t resources = state_->graph.impl_->definition->resources.size();
    state_->bound_textures.resize(resources);
    state_->bound_views.resize(resources);
    state_->bound_buffers.resize(resources);
    state_->temporal_textures.resize(resources);
    state_->temporal_views.resize(resources);
    state_->static_views.resize(resources);
    for (u32 index = 0; index < resources; ++index) {
        const auto& resource = state_->graph.impl_->definition->resources[index];
        if (resource.lifetime != ResourceLifetime::Temporal)
            continue;
        state_->temporal_textures[index] = resource.imported_texture;
        if (resource.imported_texture != nullptr)
            state_->temporal_views[index] = ref<rhi::TextureView>(resource.imported_texture
                    ->CreateView(
                        {.format = resource.default_view.format == rhi::TextureFormat::Undefined
                                       ? resource.texture.format
                                       : resource.default_view.format,
                            .dimension = resource.default_view.dimension,
                            .base_mip_level = resource.default_view.subresources.base_mip_level,
                            .mip_level_count = resource.default_view.subresources.mip_level_count
                                                       == kRemainingSubresources
                                                   ? resource.texture.mip_levels
                                                   : resource.default_view.subresources.mip_level_count,
                            .base_array_layer = resource.default_view.subresources.base_array_layer,
                            .array_layer_count = resource.default_view.subresources.array_layer_count
                                                         == kRemainingSubresources
                                                     ? resource.texture.depth_or_layers
                                                     : resource.default_view.subresources.array_layer_count,
                            .aspect = resource.default_view.subresources.aspect,
                            .usage = resource.texture.usage}
                    )
                    .release());
        if (resource.temporal_peer != kInvalidGraphIndex && index < resource.temporal_peer)
            state_->temporal_pairs.emplace_back(index, resource.temporal_peer);
    }
}

GraphExecutor::GraphExecutor(
    std::shared_ptr<GraphCommandBackend> backend,
    CompiledRenderGraph graph,
    ref<DeferredReleaseQueue> releases
)
    : state_(std::make_shared<graph_detail::ExecutorState>()) {
    state_->backend = std::move(backend);
    state_->releases = releases ? std::move(releases) : createRef<DeferredReleaseQueue>();
    state_->graph = std::move(graph);
    const size_t resources = state_->graph.impl_->definition->resources.size();
    state_->bound_textures.resize(resources);
    state_->bound_views.resize(resources);
    state_->bound_buffers.resize(resources);
    state_->temporal_textures.resize(resources);
    state_->temporal_views.resize(resources);
    state_->static_views.resize(resources);
    for (u32 index = 0; index < resources; ++index) {
        const auto& resource = state_->graph.impl_->definition->resources[index];
        if (resource.lifetime == ResourceLifetime::Temporal && resource.temporal_peer != kInvalidGraphIndex
            && index < resource.temporal_peer)
            state_->temporal_pairs.emplace_back(index, resource.temporal_peer);
    }
}

GraphExecutor::~GraphExecutor() {
    state_->valid = false;
    RetireFrame(state_);
    for (auto& retired : state_->retired)
        state_->releases->Retire(std::move(retired.physical), retired.submission);
    state_->retired.clear();
    if (state_->last_submission.IsValid()) {
        state_->releases->Retire(std::move(state_->temporal_textures), state_->last_submission);
        state_->releases->Retire(std::move(state_->temporal_views), state_->last_submission);
        state_->releases->Retire(std::move(state_->static_views), state_->last_submission);
        state_->releases->Retire(std::move(state_->graph), state_->last_submission);
    }
}

const CompiledRenderGraph& GraphExecutor::Graph() const noexcept {
    return state_->graph;
}

Result<GraphFrame> GraphExecutor::Begin(const u32 width, const u32 height) {
    auto& impl = *state_;
    auto& graph = state_->graph;
    if (!impl.valid || (impl.device == nullptr && impl.backend == nullptr))
        return Err(ErrorCode::InvalidState, "GRF2000 executor has no command backend");
    if (impl.active)
        return Err(ErrorCode::InvalidState, "GRF2001 graph recording is already active");
    if (width == 0 || height == 0)
        return Err(ErrorCode::ValidationOutOfRange, "GRF2002 frame extent must be non-zero");
    if (width != graph.impl_->width || height != graph.impl_->height)
        return Err(
            ErrorCode::ValidationInvalidState,
            "GRF2002 frame extent differs from the compiled graph extent; recompile after resize"
        );

    const rhi::SubmissionEpoch completed = impl.backend != nullptr ? impl.backend->CompletedSubmission()
                                                                   : impl.device->GetQueue().CompletedSubmission();
    for (auto retired = impl.retired.begin(); retired != impl.retired.end();) {
        if (completed.HasReached(retired->submission)) {
            impl.available.push_back(std::move(retired->physical));
            retired = impl.retired.erase(retired);
        } else {
            ++retired;
        }
    }
    static_cast<void>(impl.releases->Collect(completed));

    if (!impl.physical_acquired) {
        if (!impl.available.empty()) {
            impl.physical = std::move(impl.available.back());
            impl.available.pop_back();
        } else {
            if (impl.physical_set_count >= kMaxGraphFramesInFlight)
                return Err(ErrorCode::InvalidState, "GRF2001 graph transient pool is waiting for GPU completion");
            ++impl.physical_set_count;
        }
        impl.physical_acquired = true;
    }

    if (impl.backend == nullptr
        && (impl.width != width || impl.height != height
            || (impl.physical.textures.empty() && graph.impl_->texture_slot_count != 0))) {
        std::vector<scope<rhi::Texture>> textures(graph.impl_->texture_slot_count);
        std::vector<scope<rhi::TextureView>> views(graph.impl_->texture_slot_count);
        std::vector<scope<rhi::Buffer>> buffers(graph.impl_->buffer_slot_count);
        for (u32 resource_index = 0; resource_index < graph.impl_->definition->resources.size(); ++resource_index) {
            const auto& resource = graph.impl_->definition->resources[resource_index];
            if (!graph.impl_->retained_resources[resource_index] || resource.external)
                continue;
            const u32 slot = graph.impl_->resource_slots[resource_index];
            if (resource.type == graph_detail::ResourceType::Texture && textures[slot] == nullptr) {
                const auto extent = graph_detail::ResolveExtent(resource.texture, width, height);
                rhi::TextureDesc descriptor{.size = extent,
                    .mip_level_count = resource.texture.mip_levels,
                    .sample_count = resource.texture.sample_count,
                    .dimension = resource.texture.dimension,
                    .format = resource.texture.format,
                    .usage = resource.texture.usage,
                    .view_formats = resource.texture.view_formats,
                    .label = resource.texture.label};
                TRY_ASSIGN(textures[slot], impl.device->CreateTexture(descriptor));
                views[slot] = textures[slot]->CreateView(
                    {.format = resource.texture.format,
                        .aspect = resource.texture.view_aspect,
                        .usage = resource.texture.usage,
                        .label = resource.texture.label + ".View"}
                );
                if (views[slot] == nullptr)
                    return Err(
                        ErrorCode::GraphicsResourceCreationFailed,
                        "GRF2003 failed to create transient texture view"
                    );
            } else if (resource.type == graph_detail::ResourceType::Buffer && buffers[slot] == nullptr) {
                TRY_ASSIGN(
                    buffers[slot],
                    impl.device->CreateBuffer(
                        {.size = resource.buffer.size, .usage = resource.buffer.usage, .label = resource.buffer.label}
                    )
                );
            }
        }
        impl.physical.textures = std::move(textures);
        impl.physical.views = std::move(views);
        impl.physical.buffers = std::move(buffers);
        impl.width = width;
        impl.height = height;
    }

    for (u32 index = 0; index < graph.impl_->definition->resources.size(); ++index) {
        const auto& resource = graph.impl_->definition->resources[index];
        if (resource.lifetime == ResourceLifetime::Temporal) {
            impl.bound_textures[index] = impl.temporal_textures[index];
            impl.bound_views[index] = impl.temporal_views[index];
            continue;
        }
        if (resource.frame_bound)
            continue;
        impl.bound_textures[index] = resource.imported_texture;
        impl.bound_buffers[index] = resource.imported_buffer;
        if (resource.imported_texture != nullptr) {
            if (!SamePhysicalTexture(resource, *resource.imported_texture, graph.impl_->extents[index]))
                return Err(ErrorCode::ValidationInvalidState, "GRF2012 static texture binding metadata mismatch");
            if (impl.static_views[index] == nullptr)
                impl.static_views[index] = ref<rhi::TextureView>(resource.imported_texture
                        ->CreateView(
                            {.format = resource.default_view.format == rhi::TextureFormat::Undefined
                                           ? resource.texture.format
                                           : resource.default_view.format,
                                .dimension = resource.default_view.dimension,
                                .base_mip_level = resource.default_view.subresources.base_mip_level,
                                .mip_level_count = resource.default_view.subresources.mip_level_count
                                                           == kRemainingSubresources
                                                       ? resource.texture.mip_levels
                                                       : resource.default_view.subresources.mip_level_count,
                                .base_array_layer = resource.default_view.subresources.base_array_layer,
                                .array_layer_count = resource.default_view.subresources.array_layer_count
                                                             == kRemainingSubresources
                                                         ? resource.texture.depth_or_layers
                                                         : resource.default_view.subresources.array_layer_count,
                                .aspect = resource.default_view.subresources.aspect,
                                .usage = resource.texture.usage}
                        )
                        .release());
            impl.bound_views[index] = impl.static_views[index];
            if (impl.bound_views[index] == nullptr)
                return Err(ErrorCode::GraphicsResourceCreationFailed, "GRF2003 failed to create imported texture view");
        } else if (resource.imported_buffer != nullptr
                   && (resource.imported_buffer->GetSize() < resource.buffer.size
                       || (static_cast<u64>(resource.imported_buffer->GetUsage())
                              & static_cast<u64>(resource.buffer.usage))
                              != static_cast<u64>(resource.buffer.usage))) {
            return Err(ErrorCode::ValidationInvalidState, "GRF2012 static buffer binding metadata mismatch");
        }
    }
    if (impl.backend != nullptr)
        TRY_VOID(impl.backend->BeginFrame(width, height));
    else
        TRY_ASSIGN(impl.encoder, impl.device->CreateCommandEncoder({.label = "GfxRenderGraph"}));
    impl.width = width;
    impl.height = height;
    impl.active = true;
    return Ok(GraphFrame(state_));
}

Result<void> GraphExecutor::Bind(
    const std::shared_ptr<graph_detail::ExecutorState>& state,
    GraphTextureBinding binding
) {
    auto& impl = *state;
    auto& graph = state->graph;
    if (!impl.valid || !impl.active || !graph_detail::Belongs(*graph.impl_->definition, binding.resource)
        || binding.resource.Index() >= graph.impl_->definition->resources.size())
        return Err(ErrorCode::ValidationInvalidState, "GRF2010 invalid frame texture binding handle");
    const u32 index = binding.resource.Index();
    const auto& resource = graph.impl_->definition->resources[index];
    if (!resource.frame_bound || resource.type != graph_detail::ResourceType::Texture)
        return Err(ErrorCode::ValidationInvalidState, "GRF2011 texture resource is not frame-bound");
    if (!SameTextureSignature(
            resource.texture,
            binding.signature,
            graph.impl_->extents[index],
            graph.impl_->width,
            graph.impl_->height
        ))
        return Err(ErrorCode::ValidationInvalidState, "GRF2012 frame texture binding signature mismatch");
    if (binding.texture != nullptr) {
        if (!SamePhysicalTexture(resource, *binding.texture, graph.impl_->extents[index]))
            return Err(ErrorCode::ValidationInvalidState, "GRF2012 physical frame texture metadata mismatch");
    }
    if (binding.view == nullptr)
        return Err(ErrorCode::ValidationNullValue, "GRF2013 frame texture view is null");
    impl.bound_textures[index] = std::move(binding.texture);
    impl.bound_views[index] = std::move(binding.view);
    return Ok();
}

Result<void> GraphExecutor::Bind(
    const std::shared_ptr<graph_detail::ExecutorState>& state,
    GraphBufferBinding binding
) {
    auto& impl = *state;
    auto& graph = state->graph;
    if (!impl.valid || !impl.active || !graph_detail::Belongs(*graph.impl_->definition, binding.resource)
        || binding.resource.Index() >= graph.impl_->definition->resources.size())
        return Err(ErrorCode::ValidationInvalidState, "GRF2010 invalid frame buffer binding handle");
    const u32 index = binding.resource.Index();
    const auto& resource = graph.impl_->definition->resources[index];
    if (!resource.frame_bound || resource.type != graph_detail::ResourceType::Buffer || binding.buffer == nullptr
        || binding.signature.size != resource.buffer.size
        || (static_cast<u64>(binding.signature.usage) & static_cast<u64>(resource.buffer.usage))
               != static_cast<u64>(resource.buffer.usage)
        || binding.buffer->GetSize() < resource.buffer.size
        || (static_cast<u64>(binding.buffer->GetUsage()) & static_cast<u64>(resource.buffer.usage))
               != static_cast<u64>(resource.buffer.usage))
        return Err(ErrorCode::ValidationInvalidState, "GRF2012 frame buffer binding mismatch");
    impl.bound_buffers[index] = std::move(binding.buffer);
    return Ok();
}

Result<void> GraphExecutor::Bind(
    const std::shared_ptr<graph_detail::ExecutorState>& state,
    GraphTemporalTextureBinding binding
) {
    if (!binding.resource.previous || !binding.resource.current)
        return Err(ErrorCode::ValidationInvalidState, "GRF2010 invalid temporal binding handles");
    const u32 previous = binding.resource.previous.Index();
    const u32 current = binding.resource.current.Index();
    const auto& definition = *state->graph.impl_->definition;
    if (!state->valid || previous >= definition.resources.size() || current >= definition.resources.size()
        || !graph_detail::Belongs(definition, binding.resource.previous)
        || !graph_detail::Belongs(definition, binding.resource.current)
        || definition.resources[previous].temporal_peer != current || binding.current_view == nullptr)
        return Err(
            ErrorCode::ValidationInvalidState,
            "GRF2010 temporal binding does not match the compiled two-slot history"
        );
    if (binding.previous_view != nullptr) {
        TRY_VOID(Bind(
            state,
            GraphTextureBinding{binding.resource.previous,
                std::move(binding.previous_texture),
                std::move(binding.previous_view),
                binding.signature,
                binding.view_signature}
        ));
    } else if (state->bound_views[previous] == nullptr) {
        return Err(ErrorCode::ValidationInvalidState, "GRF2013 temporal history has no previous slot");
    }
    return Bind(
        state,
        GraphTextureBinding{binding.resource.current,
            std::move(binding.current_texture),
            std::move(binding.current_view),
            std::move(binding.signature),
            std::move(binding.view_signature)}
    );
}

Result<rhi::SubmissionTicket> GraphExecutor::ExecuteFrame(const std::shared_ptr<graph_detail::ExecutorState>& state) {
    auto& impl = *state;
    if (!impl.valid || !impl.active || (impl.encoder == nullptr && impl.backend == nullptr))
        return Err(ErrorCode::InvalidState, "GRF2020 graph frame is invalid");
    auto& compiled = *impl.graph.impl_;
    RenderGraphContext::Runtime runtime;
    runtime.device = impl.device.get();
    runtime.command = impl.encoder.get();
    runtime.definition = compiled.definition.get();
    runtime.extents = &compiled.extents;
    runtime.textures.resize(compiled.definition->resources.size());
    runtime.views.resize(compiled.definition->resources.size());
    runtime.buffers.resize(compiled.definition->resources.size());
    for (u32 resource = 0; resource < compiled.definition->resources.size(); ++resource) {
        if (!compiled.retained_resources[resource])
            continue;
        const auto& value = compiled.definition->resources[resource];
        if (!value.external && impl.backend == nullptr) {
            const u32 slot = compiled.resource_slots[resource];
            if (value.type == graph_detail::ResourceType::Texture) {
                if (slot == kInvalidGraphIndex || slot >= impl.physical.textures.size())
                    return Err(
                        ErrorCode::GraphicsResourceCreationFailed,
                        "GRF2023 retained texture has no valid physical slot"
                    );
                runtime.textures[resource] = impl.physical.textures[slot].get();
                runtime.views[resource] = impl.physical.views[slot].get();
            } else {
                if (slot == kInvalidGraphIndex || slot >= impl.physical.buffers.size())
                    return Err(
                        ErrorCode::GraphicsResourceCreationFailed,
                        "GRF2023 retained buffer has no valid physical slot"
                    );
                runtime.buffers[resource] = impl.physical.buffers[slot].get();
            }
        } else {
            runtime.textures[resource] = impl.bound_textures[resource].get();
            runtime.views[resource] = impl.bound_views[resource].get();
            runtime.buffers[resource] = impl.bound_buffers[resource].get();
        }
    }

    for (const u32 pass_index : compiled.schedule) {
        auto& pass = compiled.definition->passes[pass_index];
        for (const auto& use : pass.uses) {
            const auto& version = compiled.definition->versions[use.version];
            if (impl.backend != nullptr && !compiled.definition->resources[version.resource].external)
                continue;
            if (version.type == graph_detail::ResourceType::Texture) {
                const bool view_only = use.access == GraphAccess::ColorAttachment
                                       || use.access == GraphAccess::DepthRead || use.access == GraphAccess::DepthWrite;
                if (runtime.views[version.resource] == nullptr
                    || (!view_only && runtime.textures[version.resource] == nullptr))
                    return Err(
                        ErrorCode::GraphicsResourceCreationFailed,
                        "GRF2021 missing declared texture binding before pass"
                    );
            } else if (runtime.buffers[version.resource] == nullptr) {
                return Err(
                    ErrorCode::GraphicsResourceCreationFailed,
                    "GRF2022 missing declared buffer binding before pass"
                );
            }
        }
        RenderGraphContext context;
        context.runtime_ = &runtime;
        context.pass_ = pass_index;
        context.width_ = impl.width;
        context.height_ = impl.height;
        if (!pass.colors.empty()) {
            const u32 logical = compiled.definition->versions[pass.colors.front().version].resource;
            context.width_ = compiled.extents[logical].width;
            context.height_ = compiled.extents[logical].height;
        } else if (pass.depth) {
            const u32 logical = compiled.definition->versions[pass.depth->version].resource;
            context.width_ = compiled.extents[logical].width;
            context.height_ = compiled.extents[logical].height;
        }

        if (pass.kind == PassKind::Render) {
            if (impl.backend != nullptr) {
                TRY_VOID(impl.backend->BeginPass(pass.name, pass.kind));
                auto execute = pass.callback(context);
                impl.backend->EndPass();
                if (!execute)
                    return Err(std::move(execute.error()));
                continue;
            }
            u32 max_slot{};
            for (const auto& color : pass.colors)
                max_slot = std::max(max_slot, color.attachment.slot);
            std::vector<rhi::RenderPassColorAttachmentDesc> colors(pass.colors.empty() ? 0 : max_slot + 1);
            for (const auto& color : pass.colors) {
                const u32 logical = compiled.definition->versions[color.version].resource;
                auto view = ResolveView(runtime, logical, color.attachment.region.view);
                if (!view)
                    return Err(std::move(view).error());
                colors[color.attachment.slot] = {.view = &view->get(),
                    .load_op = color.attachment.load,
                    .store_op = color.attachment.store,
                    .clear_value = color.attachment.clear};
                if (color.attachment.resolve) {
                    const u32 resolve_logical = compiled.definition->versions[color.attachment.resolve->Index()]
                                                    .resource;
                    colors[color.attachment.slot].resolve_target = runtime.views[resolve_logical];
                }
            }
            std::optional<rhi::RenderPassDepthStencilAttachmentDesc> depth;
            if (pass.depth) {
                const u32 logical = compiled.definition->versions[pass.depth->version].resource;
                const auto& attachment = pass.depth->attachment;
                auto view = ResolveView(runtime, logical, attachment.region.view);
                if (!view)
                    return Err(std::move(view).error());
                depth = {.view = &view->get(),
                    .depth_load_op = attachment.read_only ? rhi::LoadOp::Undefined : attachment.load,
                    .depth_store_op = attachment.read_only ? rhi::StoreOp::Undefined : attachment.store,
                    .depth_clear_value = attachment.read_only ? rhi::kDepthClearValueUndefined : attachment.clear,
                    .depth_read_only = attachment.read_only,
                    .stencil_load_op = rhi::LoadOp::Undefined,
                    .stencil_store_op = rhi::StoreOp::Undefined,
                    .stencil_read_only = true};
            }
            rhi::RenderPassDescTyped descriptor{.label = pass.name,
                .color_attachments = colors,
                .depth_stencil_attachment = depth ? &*depth : nullptr};
            scope<rhi::RenderPassEncoder> encoder;
            TRY_ASSIGN(encoder, impl.encoder->BeginRenderPass(descriptor));
            runtime.render = encoder.get();
            const AttachmentRegion* region = !pass.colors.empty() ? &pass.colors.front().attachment.region
                                             : pass.depth         ? &pass.depth->attachment.region
                                                                  : nullptr;
            if (region != nullptr && region->viewport_extent) {
                const auto origin = region->viewport_origin.value_or(rhi::Origin2D{});
                encoder->SetViewport(
                    static_cast<f32>(origin.x),
                    static_cast<f32>(origin.y),
                    static_cast<f32>(region->viewport_extent->width),
                    static_cast<f32>(region->viewport_extent->height),
                    0.0F,
                    1.0F
                );
            }
            if (region != nullptr && region->scissor_extent) {
                const auto origin = region->scissor_origin.value_or(rhi::Origin2D{});
                encoder
                    ->SetScissorRect(origin.x, origin.y, region->scissor_extent->width, region->scissor_extent->height);
            }
            auto execute = pass.callback(context);
            encoder->End();
            runtime.render = nullptr;
            if (!execute) {
                impl.encoder.reset();
                return Err(std::move(execute.error()));
            }
        } else if (pass.kind == PassKind::Compute) {
            if (impl.backend != nullptr) {
                TRY_VOID(impl.backend->BeginPass(pass.name, pass.kind));
                auto execute = pass.callback(context);
                impl.backend->EndPass();
                if (!execute)
                    return Err(std::move(execute.error()));
                continue;
            }
            scope<rhi::ComputePassEncoder> encoder;
            TRY_ASSIGN(encoder, impl.encoder->BeginComputePass({.label = pass.name}));
            runtime.compute = encoder.get();
            auto execute = pass.callback(context);
            encoder->End();
            runtime.compute = nullptr;
            if (!execute) {
                impl.encoder.reset();
                return Err(std::move(execute.error()));
            }
        } else {
            if (impl.backend != nullptr) {
                TRY_VOID(impl.backend->Copy(pass.name));
                if (pass.callback) {
                    auto execute = pass.callback(context);
                    if (!execute)
                        return Err(std::move(execute.error()));
                }
                continue;
            }
            for (const auto& copy : pass.copies) {
                const auto& source_version = compiled.definition->versions[copy.source];
                const auto& destination_version = compiled.definition->versions[copy.destination];
                if (source_version.type == graph_detail::ResourceType::Texture) {
                    auto* source = runtime.textures[source_version.resource];
                    auto* destination = runtime.textures[destination_version.resource];
                    if (source == nullptr || destination == nullptr)
                        return Err(ErrorCode::GraphicsResourceCreationFailed, "GRF2022 missing texture copy binding");
                    TRY_VOID(impl.encoder->CopyTextureToTexture(
                        CopyInfo(*source),
                        CopyInfo(*destination),
                        compiled.extents[source_version.resource]
                    ));
                } else {
                    auto* source = runtime.buffers[source_version.resource];
                    auto* destination = runtime.buffers[destination_version.resource];
                    if (source == nullptr || destination == nullptr)
                        return Err(ErrorCode::GraphicsResourceCreationFailed, "GRF2022 missing buffer copy binding");
                    const u64 size = copy.size == 0 ? std::min(source->GetSize(), destination->GetSize()) : copy.size;
                    TRY_VOID(impl.encoder->CopyBufferToBuffer(*source, 0, *destination, 0, size));
                }
            }
            if (pass.callback) {
                auto execute = pass.callback(context);
                if (!execute) {
                    impl.encoder.reset();
                    return Err(std::move(execute.error()));
                }
            }
        }
    }
    if (impl.backend != nullptr) {
        TRY_VOID(impl.backend->Finish());
        auto submitted = impl.backend->Submit();
        if (submitted) {
            for (const auto [previous, current] : impl.temporal_pairs) {
                impl.temporal_textures[previous] = impl.bound_textures[current];
                impl.temporal_views[previous] = impl.bound_views[current];
                impl.temporal_textures[current] = impl.bound_textures[previous];
                impl.temporal_views[current] = impl.bound_views[previous];
            }
        }
        return submitted;
    }

    scope<rhi::CommandBuffer> command;
    TRY_ASSIGN(command, impl.encoder->Finish({.label = "GfxRenderGraphSubmit"}));
    impl.encoder.reset();
    rhi::CommandBuffer* commands[] = {command.get()};
    auto submitted = impl.device->GetQueue().Submit(commands);
    if (submitted) {
        for (const auto [previous, current] : impl.temporal_pairs) {
            impl.temporal_textures[previous] = impl.bound_textures[current];
            impl.temporal_views[previous] = impl.bound_views[current];
            impl.temporal_textures[current] = impl.bound_textures[previous];
            impl.temporal_views[current] = impl.bound_views[previous];
        }
    }
    return submitted;
}

void GraphExecutor::RetireFrame(
    const std::shared_ptr<graph_detail::ExecutorState>& state,
    const rhi::SubmissionTicket submission
) noexcept {
    state->encoder.reset();
    std::ranges::fill(state->bound_textures, nullptr);
    std::ranges::fill(state->bound_views, nullptr);
    std::ranges::fill(state->bound_buffers, nullptr);
    if (submission.IsValid() && state->physical_acquired) {
        state->retired.push_back({submission, std::move(state->physical)});
        state->physical_acquired = false;
        state->last_submission = submission;
    }
    state->active = false;
}

GraphFrame::GraphFrame(std::shared_ptr<graph_detail::ExecutorState> state)
    : state_(std::move(state)) {}

GraphFrame::GraphFrame(GraphFrame&& other) noexcept
    : state_(std::move(other.state_)),
      executed_(other.executed_) {}

GraphFrame& GraphFrame::operator=(GraphFrame&& other) noexcept {
    if (this != &other) {
        Release();
        state_ = std::move(other.state_);
        executed_ = other.executed_;
    }
    return *this;
}

GraphFrame::~GraphFrame() {
    Release();
}

Result<void> GraphFrame::Bind(GraphTextureBinding binding) {
    return state_ == nullptr || !state_->valid ? Err(ErrorCode::InvalidState, "GRF2030 frame is retired")
                                               : GraphExecutor::Bind(state_, std::move(binding));
}

Result<void> GraphFrame::Bind(GraphBufferBinding binding) {
    return state_ == nullptr || !state_->valid ? Err(ErrorCode::InvalidState, "GRF2030 frame is retired")
                                               : GraphExecutor::Bind(state_, std::move(binding));
}

Result<void> GraphFrame::Bind(GraphTemporalTextureBinding binding) {
    return state_ == nullptr || !state_->valid ? Err(ErrorCode::InvalidState, "GRF2030 frame is retired")
                                               : GraphExecutor::Bind(state_, std::move(binding));
}

Result<rhi::SubmissionTicket> GraphFrame::Execute() {
    if (state_ == nullptr || !state_->valid || executed_)
        return Err(ErrorCode::InvalidState, "GRF2031 frame is invalid or already executed");
    executed_ = true;
    auto result = GraphExecutor::ExecuteFrame(state_);
    GraphExecutor::RetireFrame(state_, result ? *result : rhi::SubmissionTicket{});
    state_.reset();
    return result;
}

void GraphFrame::Release() noexcept {
    if (state_ != nullptr) {
        GraphExecutor::RetireFrame(state_);
        state_.reset();
    }
}

} // namespace woki::gfx
