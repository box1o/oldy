#include <utility>
#include <algorithm>

#include <woki/rhi/queue.hpp>
#include <woki/rhi/device.hpp>
#include <woki/rhi/objects.hpp>
#include <woki/rhi/render_graph.hpp>
#include <woki/rhi/command_encoder.hpp>
#include <woki/rhi/render_pass_encoder.hpp>

namespace woki::rhi {
namespace {

using render_graph::detail::ColorOutput;
using render_graph::detail::CopyOperation;
using render_graph::detail::DepthOutput;
using render_graph::detail::FramebufferRecord;
using render_graph::detail::GraphBlueprint;
using render_graph::detail::PassKind;
using render_graph::detail::PassRecord;
using render_graph::detail::PooledTransientTexture;
using render_graph::detail::ResourceKind;
using render_graph::detail::ResourceRecord;
using render_graph::detail::SampleInput;
using render_graph::detail::TransientPoolKey;

[[nodiscard]] Extent3D ResolveExtent(const ExtentMode& mode, const u32 width, const u32 height) {
    switch (mode.kind) {
        case ExtentModeKind::Swapchain:
            return Extent3D{width, height, 1};
        case ExtentModeKind::Fixed:
            return Extent3D{mode.width, mode.height, 1};
        case ExtentModeKind::Relative:
            return Extent3D{
                std::max(1u, static_cast<u32>(static_cast<f32>(width) * mode.relative_width)),
                std::max(1u, static_cast<u32>(static_cast<f32>(height) * mode.relative_height)),
                1,
            };
    }
    return Extent3D{width, height, 1};
}

[[nodiscard]] bool IsDepthFormat(const TextureFormat format) noexcept {
    switch (format) {
        case TextureFormat::Depth16Unorm:
        case TextureFormat::Depth24Plus:
        case TextureFormat::Depth24PlusStencil8:
        case TextureFormat::Depth32Float:
        case TextureFormat::Depth32FloatStencil8:
            return true;
        default:
            return false;
    }
}

[[nodiscard]] TextureViewDesc MakeTransientViewDesc(const TransientDesc& desc) {
    TextureViewDesc view_desc{};
    view_desc.label = desc.label.empty() ? "RenderGraphView" : desc.label;
    view_desc.format = desc.format;
    view_desc.usage = desc.usage;
    return view_desc;
}

[[nodiscard]] TextureViewDesc MakeDepthSampleViewDesc(const TransientDesc& desc) {
    TextureViewDesc view_desc{};
    view_desc.label = desc.label.empty() ? "RenderGraphDepthSample" : desc.label + ".DepthSample";
    view_desc.format = TextureFormat::Undefined;
    view_desc.usage = desc.usage;
    view_desc.aspect = TextureAspect::DepthOnly;
    return view_desc;
}

[[nodiscard]] Result<scope<Texture>> CreateTransientTexture(Device& device, const TransientDesc& desc, const u32 width, const u32 height) {
    TextureDesc native_desc{};
    native_desc.label = desc.label.empty() ? "RenderGraphTransient" : desc.label;
    native_desc.size = ResolveExtent(desc.extent, width, height);
    native_desc.format = desc.format;
    native_desc.usage = desc.usage;
    native_desc.dimension = TextureDimension::e2D;
    native_desc.mip_level_count = 1;
    native_desc.sample_count = 1;
    return device.CreateTexture(native_desc);
}

[[nodiscard]] TransientPoolKey MakePoolKey(const TransientDesc& desc, const u32 width, const u32 height) {
    const Extent3D size = ResolveExtent(desc.extent, width, height);
    return TransientPoolKey{
        .format = desc.format,
        .usage = desc.usage,
        .width = size.width,
        .height = size.height,
    };
}

[[nodiscard]] TexelCopyTextureInfo MakeCopyInfo(Texture& texture) {
    return TexelCopyTextureInfo{
        .texture = texture.GetNativeHandles().resource,
        .mip_level = 0,
        .origin = {},
        .aspect = TextureAspect::All,
    };
}

} // namespace

// --- RenderPassContext ---

RenderPassEncoder& RenderPassContext::encoder() {
    WOKI_ASSERT(pass_ != nullptr);
    return *pass_;
}

Device& RenderPassContext::device() noexcept {
    WOKI_ASSERT(device_ != nullptr);
    return *device_;
}

const ref<Device>& RenderPassContext::device_ref() const noexcept {
    return device_;
}

TextureView& RenderPassContext::color(const u32 slot) {
    WOKI_ASSERT(slot < colors_.size() && colors_[slot] != nullptr);
    return *colors_[slot];
}

TextureView& RenderPassContext::depth() {
    WOKI_ASSERT(depth_ != nullptr);
    return *depth_;
}

bool RenderPassContext::has_depth() const noexcept {
    return depth_ != nullptr;
}

TextureView& RenderPassContext::sample(const u32 slot) {
    WOKI_ASSERT(slot < samples_.size() && samples_[slot] != nullptr);
    return *samples_[slot];
}

u32 RenderPassContext::sample_count() const noexcept {
    return static_cast<u32>(samples_.size());
}

BindGroup* RenderPassContext::GetOrCreateBindGroup(const std::string_view key, std::function<scope<BindGroup>()> factory) {
    const std::string cache_key(key);
    if (auto it = bind_group_cache_.find(cache_key); it != bind_group_cache_.end()) {
        return it->second.get();
    }

    auto created = factory();
    if (!created) {
        return nullptr;
    }

    BindGroup* raw = created.get();
    bind_group_cache_.emplace(cache_key, std::move(created));
    return raw;
}

// --- BindGroupBuilder ---

BindGroupBuilder::BindGroupBuilder(ref<Device> device, ref<BindGroupLayout> layout, const std::string_view label)
    : device_(std::move(device)),
      layout_(std::move(layout)),
      label_(label) {}

BindGroupBuilder& BindGroupBuilder::BindTexture(const u32 binding, TextureView& view) {
    entries_.push_back(BindGroupEntryDesc{
        .binding = binding,
        .texture_view = &view,
    });
    return *this;
}

BindGroupBuilder& BindGroupBuilder::BindSampler(const u32 binding, Sampler& sampler) {
    entries_.push_back(BindGroupEntryDesc{
        .binding = binding,
        .sampler = &sampler,
    });
    return *this;
}

BindGroupBuilder& BindGroupBuilder::BindBuffer(const u32 binding, Buffer& buffer, const u64 offset, const u64 size) {
    entries_.push_back(BindGroupEntryDesc{
        .binding = binding,
        .buffer = &buffer,
        .offset = offset,
        .size = size,
    });
    return *this;
}

Result<scope<BindGroup>> BindGroupBuilder::Build() {
    if (device_ == nullptr || layout_ == nullptr) {
        return Err(ErrorCode::InvalidState, "BindGroupBuilder is invalid");
    }

    BindGroupDesc desc{};
    desc.label = label_;
    desc.layout = layout_.get();
    desc.entries = entries_;
    return device_->CreateBindGroup(desc);
}

// --- CopyPassContext ---

CommandEncoder& CopyPassContext::encoder() {
    WOKI_ASSERT(encoder_ != nullptr);
    return *encoder_;
}

Device& CopyPassContext::device() noexcept {
    WOKI_ASSERT(device_ != nullptr);
    return *device_;
}

const ref<Device>& CopyPassContext::device_ref() const noexcept {
    return device_;
}

Texture& CopyPassContext::src(const u32 index) {
    WOKI_ASSERT(index < sources_.size() && sources_[index] != nullptr);
    return *sources_[index];
}

Texture& CopyPassContext::dst(const u32 index) {
    WOKI_ASSERT(index < destinations_.size() && destinations_[index] != nullptr);
    return *destinations_[index];
}

Result<void> CopyPassContext::CopyAll() {
    if (encoder_ == nullptr) {
        return Err(ErrorCode::InvalidState, "CopyPassContext has no encoder");
    }
    if (sources_.size() != destinations_.size()) {
        return Err(ErrorCode::ValidationInvalidState, "CopyPassContext source/destination mismatch");
    }

    for (size_t i = 0; i < sources_.size(); ++i) {
        Texture* source = sources_[i];
        Texture* destination = destinations_[i];
        if (source == nullptr || destination == nullptr) {
            return Err(ErrorCode::GraphicsResourceCreationFailed, "CopyPassContext missing texture");
        }

        const Extent3D copy_size{
            std::max(1u, width_),
            std::max(1u, height_),
            1,
        };
        if (auto result = encoder_->CopyTextureToTexture(MakeCopyInfo(*source), MakeCopyInfo(*destination), copy_size); !result) {
            return result;
        }
    }

    return Ok();
}

// --- PassBuilder ---

PassBuilder::PassBuilder(ref<GraphBlueprint> blueprint, const u32 pass_index)
    : blueprint_(std::move(blueprint)),
      pass_index_(pass_index) {}

PassBuilder& PassBuilder::Target(const Framebuffer framebuffer, FramebufferTargetConfig config) {
    WOKI_ASSERT(blueprint_ != nullptr);
    WOKI_ASSERT(framebuffer);
    WOKI_ASSERT(pass_index_ < blueprint_->passes.size());

    PassRecord& pass = blueprint_->passes[pass_index_];
    pass.framebuffer_id = framebuffer.id_;
    pass.framebuffer_config = std::move(config);
    return *this;
}

PassBuilder& PassBuilder::Color(const u32 slot, const Resource resource, ColorAttachmentConfig config) {
    WOKI_ASSERT(blueprint_ != nullptr);
    WOKI_ASSERT(resource);
    WOKI_ASSERT(pass_index_ < blueprint_->passes.size());

    blueprint_->passes[pass_index_].colors.push_back(ColorOutput{
        .slot = slot,
        .resource_id = resource.id_,
        .config = config,
    });
    return *this;
}

PassBuilder& PassBuilder::Color(const u32 slot, const PerFrameSlot resource, ColorAttachmentConfig config) {
    WOKI_ASSERT(blueprint_ != nullptr);
    WOKI_ASSERT(resource);
    WOKI_ASSERT(pass_index_ < blueprint_->passes.size());

    blueprint_->passes[pass_index_].colors.push_back(ColorOutput{
        .slot = slot,
        .resource_id = resource.id_,
        .config = config,
    });
    return *this;
}

PassBuilder& PassBuilder::Depth(const Resource resource, DepthAttachmentConfig config) {
    WOKI_ASSERT(blueprint_ != nullptr);
    WOKI_ASSERT(resource);
    WOKI_ASSERT(pass_index_ < blueprint_->passes.size());

    blueprint_->passes[pass_index_].depth = DepthOutput{
        .resource_id = resource.id_,
        .config = config,
    };
    return *this;
}

PassBuilder& PassBuilder::Depth(const PerFrameSlot resource, DepthAttachmentConfig config) {
    WOKI_ASSERT(blueprint_ != nullptr);
    WOKI_ASSERT(resource);
    WOKI_ASSERT(pass_index_ < blueprint_->passes.size());

    blueprint_->passes[pass_index_].depth = DepthOutput{
        .resource_id = resource.id_,
        .config = config,
    };
    return *this;
}

PassBuilder& PassBuilder::Sample(const Resource resource, const SampleMode mode) {
    WOKI_ASSERT(blueprint_ != nullptr);
    WOKI_ASSERT(resource);
    WOKI_ASSERT(pass_index_ < blueprint_->passes.size());

    blueprint_->passes[pass_index_].samples.push_back(SampleInput{
        .resource_id = resource.id_,
        .mode = mode,
    });
    return *this;
}

PassBuilder& PassBuilder::Copy(const Resource src, const Resource dst) {
    WOKI_ASSERT(blueprint_ != nullptr);
    WOKI_ASSERT(src);
    WOKI_ASSERT(dst);
    WOKI_ASSERT(pass_index_ < blueprint_->passes.size());

    PassRecord& pass = blueprint_->passes[pass_index_];
    pass.kind = PassKind::Copy;
    pass.copies.push_back(CopyOperation{
        .src_resource_id = src.id_,
        .dst_resource_id = dst.id_,
    });
    return *this;
}

// --- FramebufferBuilder ---

FramebufferBuilder::FramebufferBuilder(ref<GraphBlueprint> blueprint, const u32 framebuffer_index)
    : blueprint_(std::move(blueprint)),
      framebuffer_index_(framebuffer_index) {}

FramebufferBuilder& FramebufferBuilder::Color(const u32 slot, const Resource resource) {
    WOKI_ASSERT(blueprint_ != nullptr);
    WOKI_ASSERT(resource);
    WOKI_ASSERT(framebuffer_index_ < blueprint_->framebuffers.size());

    blueprint_->framebuffers[framebuffer_index_].colors.emplace_back(slot, resource.id_);
    return *this;
}

FramebufferBuilder& FramebufferBuilder::Depth(const Resource resource) {
    WOKI_ASSERT(blueprint_ != nullptr);
    WOKI_ASSERT(resource);
    WOKI_ASSERT(framebuffer_index_ < blueprint_->framebuffers.size());

    blueprint_->framebuffers[framebuffer_index_].depth_resource_id = resource.id_;
    return *this;
}

Framebuffer FramebufferBuilder::Build() {
    Framebuffer framebuffer{};
    framebuffer.id_ = framebuffer_index_;
    return framebuffer;
}

// --- RenderGraphBuilder ---

RenderGraphBuilder::RenderGraphBuilder(ref<Device> device)
    : device_(std::move(device)) {}

PerFrameSlot RenderGraphBuilder::PerFrame() {
    PerFrameSlot slot{};
    slot.id_ = AllocateResource(ResourceRecord{.kind = ResourceKind::PerFrame});
    return slot;
}

Resource RenderGraphBuilder::Transient(TransientDesc desc) {
    Resource resource{};
    resource.id_ = AllocateResource(ResourceRecord{
        .kind = ResourceKind::Transient,
        .transient = std::move(desc),
    });
    return resource;
}

Resource RenderGraphBuilder::Use(ref<Texture> texture) {
    Resource resource{};
    resource.id_ = AllocateResource(ResourceRecord{
        .kind = ResourceKind::Owned,
        .owned_texture = std::move(texture),
    });
    return resource;
}

FramebufferBuilder RenderGraphBuilder::Framebuffer() {
    const u32 id = AllocateFramebuffer();
    return FramebufferBuilder(blueprint_, id);
}

PassBuilder RenderGraphBuilder::AddPass(const std::string_view debug_name) {
    const u32 id = AllocatePass(debug_name);
    return PassBuilder(blueprint_, id);
}

Result<ref<RenderGraph>> RenderGraphBuilder::Compile(const u32 width, const u32 height) {
    if (device_ == nullptr) {
        return Err(ErrorCode::GraphicsInitFailed, "RenderGraphBuilder has no device");
    }
    if (width == 0 || height == 0) {
        return Err(ErrorCode::ValidationOutOfRange, "RenderGraph compile requires non-zero size");
    }

    for (const PassRecord& pass : blueprint_->passes) {
        if (pass.kind == PassKind::Copy || !pass.copies.empty()) {
            if (pass.copies.empty()) {
                return Err(ErrorCode::ValidationInvalidState, "RenderGraph copy pass '" + pass.debug_name + "' has no Copy operations");
            }
            if (!pass.copy_execute) {
                return Err(ErrorCode::ValidationInvalidState, "RenderGraph copy pass '" + pass.debug_name + "' has no Execute callback");
            }
            continue;
        }

        if (!pass.render_execute) {
            return Err(ErrorCode::ValidationInvalidState, "RenderGraph pass '" + pass.debug_name + "' has no Execute callback");
        }
    }

    return RenderGraph::Create(device_, std::move(*blueprint_), width, height);
}

u32 RenderGraphBuilder::AllocateResource(ResourceRecord record) {
    const u32 id = static_cast<u32>(blueprint_->resources.size());
    blueprint_->resources.push_back(std::move(record));
    return id;
}

u32 RenderGraphBuilder::AllocateFramebuffer() {
    const u32 id = static_cast<u32>(blueprint_->framebuffers.size());
    blueprint_->framebuffers.emplace_back();
    return id;
}

u32 RenderGraphBuilder::AllocatePass(const std::string_view debug_name) {
    const u32 id = static_cast<u32>(blueprint_->passes.size());
    blueprint_->passes.push_back(PassRecord{.debug_name = std::string(debug_name)});
    blueprint_->pass_name_to_index.emplace(blueprint_->passes.back().debug_name, id);
    return id;
}

// --- RenderGraph ---

Result<ref<RenderGraph>> RenderGraph::Create(ref<Device> device, GraphBlueprint blueprint, const u32 width, const u32 height) {
    return Ok(createRef<RenderGraph>(ConstructionKey{}, std::move(device), std::move(blueprint), width, height));
}

RenderGraph::RenderGraph(ConstructionKey, ref<Device> device, GraphBlueprint blueprint, const u32 width, const u32 height)
    : device_(std::move(device)),
      blueprint_(std::move(blueprint)),
      width_(width),
      height_(height) {
    runtime_resources_.resize(blueprint_.resources.size());
    for (size_t i = 0; i < blueprint_.resources.size(); ++i) {
        runtime_resources_[i].blueprint = blueprint_.resources[i];
    }
    (void)AllocateRuntimeResources(width, height);
}

Result<void> RenderGraph::AllocateRuntimeResources(const u32 width, const u32 height) {
    width_ = width;
    height_ = height;

    for (RuntimeResource& runtime : runtime_resources_) {
        if (runtime.blueprint.kind != ResourceKind::Transient) {
            continue;
        }
        if (auto result = AcquireTransientResource(runtime, width, height); !result) {
            return result;
        }
    }

    for (RuntimeResource& runtime : runtime_resources_) {
        const ResourceRecord& record = runtime.blueprint;

        if (record.kind == ResourceKind::Owned && record.owned_texture != nullptr) {
            if (runtime.view == nullptr) {
                runtime.view = record.owned_texture->CreateView({});
            }
        }
    }

    return Ok();
}

void RenderGraph::ReleaseTransientPool() {
    for (PooledTransientTexture& entry : transient_pool_) {
        entry.in_use = false;
    }

    for (RuntimeResource& runtime : runtime_resources_) {
        if (runtime.blueprint.kind == ResourceKind::Transient) {
            runtime.pool_index = kInvalidGraphResource;
            runtime.texture.reset();
            runtime.view.reset();
        }
    }
}

Result<void> RenderGraph::AcquireTransientResource(RuntimeResource& runtime, const u32 width, const u32 height) {
    const ResourceRecord& record = runtime.blueprint;
    const TransientPoolKey key = MakePoolKey(record.transient, width, height);

    for (u32 pool_index = 0; pool_index < transient_pool_.size(); ++pool_index) {
        PooledTransientTexture& entry = transient_pool_[pool_index];
        if (entry.in_use || entry.key != key) {
            continue;
        }

        entry.in_use = true;
        runtime.pool_index = pool_index;
        runtime.texture.reset();
        runtime.view.reset();
        return Ok();
    }

    auto texture = CreateTransientTexture(*device_, record.transient, width, height);
    if (!texture) {
        return Err(texture.error());
    }

    TextureViewDesc view_desc = MakeTransientViewDesc(record.transient);

    PooledTransientTexture pooled{};
    pooled.key = key;
    pooled.texture = std::move(*texture);
    pooled.view = pooled.texture->CreateView(view_desc);
    if (IsDepthFormat(record.transient.format)) {
        pooled.depth_sample_view = pooled.texture->CreateView(MakeDepthSampleViewDesc(record.transient));
    }
    pooled.in_use = true;

    runtime.pool_index = static_cast<u32>(transient_pool_.size());
    transient_pool_.push_back(std::move(pooled));
    return Ok();
}

Result<void> RenderGraph::RebuildForResize(const u32 width, const u32 height) {
    if (width == 0 || height == 0) {
        return Err(ErrorCode::ValidationOutOfRange, "RenderGraph resize requires non-zero size");
    }

    ReleaseTransientPool();
    return AllocateRuntimeResources(width, height);
}

Texture* RenderGraph::ResolveTexture(const u32 resource_id) {
    if (resource_id >= runtime_resources_.size()) {
        return nullptr;
    }

    RuntimeResource& runtime = runtime_resources_[resource_id];
    switch (runtime.blueprint.kind) {
        case ResourceKind::Transient:
            if (runtime.pool_index < transient_pool_.size()) {
                return transient_pool_[runtime.pool_index].texture.get();
            }
            return runtime.texture.get();
        case ResourceKind::Owned:
            return runtime.blueprint.owned_texture.get();
        case ResourceKind::PerFrame:
            return nullptr;
    }
    return nullptr;
}

TextureView* RenderGraph::ResolveView(const u32 resource_id) {
    if (resource_id >= runtime_resources_.size()) {
        return nullptr;
    }

    RuntimeResource& runtime = runtime_resources_[resource_id];
    switch (runtime.blueprint.kind) {
        case ResourceKind::Transient:
            if (runtime.pool_index < transient_pool_.size()) {
                return transient_pool_[runtime.pool_index].view.get();
            }
            return runtime.view.get();
        case ResourceKind::PerFrame:
            return nullptr;
        case ResourceKind::Owned:
            return runtime.view.get();
    }
    return nullptr;
}

TextureView* RenderGraph::ResolveSampleView(const u32 resource_id, const SampleMode mode) {
    if (mode == SampleMode::DepthTexture && resource_id < runtime_resources_.size()) {
        RuntimeResource& runtime = runtime_resources_[resource_id];
        if (runtime.blueprint.kind == ResourceKind::Transient && runtime.pool_index < transient_pool_.size()) {
            TextureView* depth_view = transient_pool_[runtime.pool_index].depth_sample_view.get();
            if (depth_view != nullptr) {
                return depth_view;
            }
        }
    }
    return ResolveView(resource_id);
}

Result<void> RenderGraph::ExecuteRenderPass(const u32 pass_index, CommandEncoder& encoder, const u32 width, const u32 height, const std::unordered_map<u32, ref<TextureView>>& per_frame_views) {
    const PassRecord& pass = blueprint_.passes[pass_index];

    std::vector<RenderPassColorAttachmentDesc> color_attachments{};
    std::optional<RenderPassDepthStencilAttachmentDesc> depth_attachment{};

    auto resolve = [&](const u32 resource_id) -> TextureView* {
        if (resource_id < runtime_resources_.size()) {
            RuntimeResource& runtime = runtime_resources_[resource_id];
            if (runtime.blueprint.kind == ResourceKind::PerFrame) {
                if (const auto it = per_frame_views.find(resource_id); it != per_frame_views.end()) {
                    return it->second.get();
                }
                return nullptr;
            }
        }
        return ResolveView(resource_id);
    };

    if (pass.framebuffer_id.has_value() && *pass.framebuffer_id < blueprint_.framebuffers.size()) {
        const FramebufferRecord& framebuffer = blueprint_.framebuffers[*pass.framebuffer_id];

        u32 max_slot = 0;
        for (const auto& [slot, resource_id] : framebuffer.colors) {
            (void)resource_id;
            max_slot = std::max(max_slot, slot);
        }
        if (!framebuffer.colors.empty()) {
            color_attachments.assign(max_slot + 1, RenderPassColorAttachmentDesc{});
        }

        for (const auto& [slot, resource_id] : framebuffer.colors) {
            TextureView* view = resolve(resource_id);
            if (view == nullptr) {
                return Err(ErrorCode::GraphicsResourceCreationFailed, "RenderGraph pass '" + pass.debug_name + "' missing color view for slot " + std::to_string(slot));
            }

            Color clear_value{0.f, 0.f, 0.f, 1.f};
            if (slot < pass.framebuffer_config.clear_color.size()) {
                clear_value = pass.framebuffer_config.clear_color[slot];
            }

            color_attachments[slot] = RenderPassColorAttachmentDesc{
                .view = view,
                .load_op = LoadOp::Clear,
                .store_op = StoreOp::Store,
                .clear_value = clear_value,
            };
        }

        if (framebuffer.depth_resource_id != kInvalidGraphResource) {
            TextureView* depth_view = resolve(framebuffer.depth_resource_id);
            if (depth_view == nullptr) {
                return Err(ErrorCode::GraphicsResourceCreationFailed, "RenderGraph pass '" + pass.debug_name + "' missing depth view");
            }

            depth_attachment = RenderPassDepthStencilAttachmentDesc{
                .view = depth_view,
                .depth_load_op = LoadOp::Clear,
                .depth_store_op = StoreOp::Store,
                .depth_clear_value = pass.framebuffer_config.clear_depth,
            };
        }
    }

    if (!pass.colors.empty()) {
        u32 max_color_slot = 0;
        for (const ColorOutput& color : pass.colors) {
            max_color_slot = std::max(max_color_slot, color.slot);
        }
        if (color_attachments.size() < max_color_slot + 1) {
            color_attachments.resize(max_color_slot + 1, RenderPassColorAttachmentDesc{});
        }
    }

    for (const ColorOutput& color : pass.colors) {
        TextureView* view = resolve(color.resource_id);
        if (view == nullptr) {
            return Err(ErrorCode::GraphicsResourceCreationFailed, "RenderGraph pass '" + pass.debug_name + "' missing color attachment view");
        }

        color_attachments[color.slot] = RenderPassColorAttachmentDesc{
            .view = view,
            .load_op = color.config.load,
            .store_op = color.config.store,
            .clear_value = color.config.clear,
        };
    }

    if (pass.depth.has_value()) {
        TextureView* depth_view = resolve(pass.depth->resource_id);
        if (depth_view == nullptr) {
            return Err(ErrorCode::GraphicsResourceCreationFailed, "RenderGraph pass '" + pass.debug_name + "' missing depth attachment view");
        }

        depth_attachment = RenderPassDepthStencilAttachmentDesc{
            .view = depth_view,
            .depth_load_op = pass.depth->config.load,
            .depth_store_op = pass.depth->config.store,
            .depth_clear_value = pass.depth->config.clear,
            .depth_read_only = !pass.depth->config.write,
        };
    }

    if (color_attachments.empty() && !depth_attachment.has_value()) {
        return Err(ErrorCode::ValidationInvalidState, "RenderGraph pass '" + pass.debug_name + "' has no render targets");
    }

    RenderPassDescTyped pass_desc{};
    pass_desc.label = pass.debug_name;
    pass_desc.color_attachments = color_attachments;
    pass_desc.depth_stencil_attachment = depth_attachment.has_value() ? &*depth_attachment : nullptr;

    auto pass_encoder = encoder.BeginRenderPass(pass_desc);
    if (!pass_encoder) {
        return Err(pass_encoder.error());
    }

    RenderPassContext context{};
    context.pass_ = pass_encoder->get();
    context.device_ = device_;
    context.user_data_ = pass.user_data;
    context.width_ = width;
    context.height_ = height;

    context.colors_.resize(color_attachments.size());
    for (size_t i = 0; i < color_attachments.size(); ++i) {
        context.colors_[i] = color_attachments[i].view;
    }
    context.depth_ = depth_attachment.has_value() ? depth_attachment->view : nullptr;

    context.samples_.reserve(pass.samples.size());
    for (const SampleInput& sample : pass.samples) {
        TextureView* view = ResolveSampleView(sample.resource_id, sample.mode);
        if (view == nullptr) {
            return Err(ErrorCode::GraphicsResourceCreationFailed, "RenderGraph pass '" + pass.debug_name + "' missing sample view");
        }
        context.samples_.push_back(view);
    }

    pass.render_execute(context);
    pass_encoder->get()->End();
    return Ok();
}

Result<void> RenderGraph::ExecuteCopyPass(const u32 pass_index, CommandEncoder& encoder, const u32 width, const u32 height) {
    const PassRecord& pass = blueprint_.passes[pass_index];

    CopyPassContext context{};
    context.encoder_ = &encoder;
    context.device_ = device_;
    context.user_data_ = pass.user_data;
    context.width_ = width;
    context.height_ = height;

    context.sources_.reserve(pass.copies.size());
    context.destinations_.reserve(pass.copies.size());
    for (const CopyOperation& copy : pass.copies) {
        Texture* source = ResolveTexture(copy.src_resource_id);
        Texture* destination = ResolveTexture(copy.dst_resource_id);
        if (source == nullptr || destination == nullptr) {
            return Err(ErrorCode::GraphicsResourceCreationFailed, "RenderGraph copy pass '" + pass.debug_name + "' missing texture");
        }
        context.sources_.push_back(source);
        context.destinations_.push_back(destination);
    }

    pass.copy_execute(context);
    return Ok();
}

RenderGraphFrame RenderGraph::BeginFrame(const u32 width, const u32 height) {
    if (width != width_ || height != height_) {
        (void)RebuildForResize(width, height);
    }

    RenderGraphFrame frame(shared_from_this(), width, height);
    auto encoder = device_->CreateCommandEncoder({.label = "RenderGraphFrame"});
    if (encoder) {
        frame.encoder_ = std::move(*encoder);
    }
    return frame;
}

// --- RenderGraphFrame ---

RenderGraphFrame::RenderGraphFrame(ref<RenderGraph> graph, const u32 width, const u32 height)
    : graph_(std::move(graph)),
      width_(width),
      height_(height) {}

RenderGraphFrame::~RenderGraphFrame() = default;

void RenderGraphFrame::Bind(const PerFrameSlot slot, ref<TextureView> view) {
    if (!slot || graph_ == nullptr) {
        return;
    }

    per_frame_views_[slot.id_] = std::move(view);
}

Result<void> RenderGraphFrame::Execute() {
    if (graph_ == nullptr || graph_->device_ == nullptr || !encoder_) {
        return Err(ErrorCode::InvalidState, "RenderGraphFrame is invalid");
    }

    for (size_t pass_index = 0; pass_index < graph_->blueprint_.passes.size(); ++pass_index) {
        const PassRecord& pass = graph_->blueprint_.passes[pass_index];
        Result<void> result = Ok();
        if (pass.kind == PassKind::Copy || !pass.copies.empty()) {
            result = graph_->ExecuteCopyPass(static_cast<u32>(pass_index), *encoder_, width_, height_);
        } else {
            result = graph_->ExecuteRenderPass(static_cast<u32>(pass_index), *encoder_, width_, height_, per_frame_views_);
        }
        if (!result) {
            return result;
        }
    }

    auto command_buffer = encoder_->Finish({.label = "RenderGraphSubmit"});
    if (!command_buffer) {
        return Err(command_buffer.error());
    }

    CommandBuffer* buffers[] = {command_buffer->get()};
    return graph_->device_->GetQueue().Submit(buffers);
}

} // namespace woki::rhi
