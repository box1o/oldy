#include <cmath>
#include <limits>
#include <utility>
#include <algorithm>
#include <unordered_set>

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

[[nodiscard]] TextureUsage ResourceUsage(const ResourceRecord& resource) noexcept {
    if (resource.kind == ResourceKind::Transient) {
        return resource.transient.usage;
    }
    if (resource.kind == ResourceKind::Owned && resource.owned_texture != nullptr) {
        return resource.owned_texture->GetUsage();
    }
    return TextureUsage::None;
}

[[nodiscard]] TextureFormat ResourceFormat(const ResourceRecord& resource) noexcept {
    if (resource.kind == ResourceKind::Transient) {
        return resource.transient.format;
    }
    if (resource.kind == ResourceKind::Owned && resource.owned_texture != nullptr) {
        return resource.owned_texture->GetFormat();
    }
    return TextureFormat::Undefined;
}

[[nodiscard]] TextureDimension ResourceDimension(const ResourceRecord& resource) noexcept {
    if (resource.kind == ResourceKind::Transient) {
        return TextureDimension::e2D;
    }
    return resource.kind == ResourceKind::Owned && resource.owned_texture != nullptr ? resource.owned_texture->GetDimension() : TextureDimension::Undefined;
}

[[nodiscard]] u32 ResourceSampleCount(const ResourceRecord& resource) noexcept {
    if (resource.kind == ResourceKind::Transient) {
        return 1;
    }
    return resource.kind == ResourceKind::Owned && resource.owned_texture != nullptr ? resource.owned_texture->GetSampleCount() : 0;
}

enum class FormatAspects : u8 {
    Color,
    Depth,
    Stencil,
    DepthStencil,
};

[[nodiscard]] FormatAspects ResourceAspects(const TextureFormat format) noexcept {
    switch (format) {
        case TextureFormat::Stencil8:
            return FormatAspects::Stencil;
        case TextureFormat::Depth24PlusStencil8:
        case TextureFormat::Depth32FloatStencil8:
            return FormatAspects::DepthStencil;
        case TextureFormat::Depth16Unorm:
        case TextureFormat::Depth24Plus:
        case TextureFormat::Depth32Float:
            return FormatAspects::Depth;
        default:
            return FormatAspects::Color;
    }
}

[[nodiscard]] Extent3D ResourceExtent(const ResourceRecord& resource, const u32 width, const u32 height) noexcept {
    if (resource.kind == ResourceKind::Transient) {
        return ResolveExtent(resource.transient.extent, width, height);
    }
    if (resource.kind == ResourceKind::Owned && resource.owned_texture != nullptr) {
        return {resource.owned_texture->GetWidth(), resource.owned_texture->GetHeight(), resource.owned_texture->GetDepthOrArrayLayers()};
    }
    return {};
}

[[nodiscard]] bool ExtentsEqual(const Extent3D& lhs, const Extent3D& rhs) noexcept {
    return lhs.width == rhs.width && lhs.height == rhs.height && lhs.depth_or_array_layers == rhs.depth_or_array_layers;
}

[[nodiscard]] Result<void> ValidateCopyCompatibility(const ResourceRecord& source, const ResourceRecord& destination, const u32 width, const u32 height) {
    if (ResourceFormat(source) != ResourceFormat(destination)) {
        return Err(ErrorCode::GraphicsInvalidFormat, "RenderGraph copy source and destination formats differ");
    }
    if (ResourceAspects(ResourceFormat(source)) != ResourceAspects(ResourceFormat(destination))) {
        return Err(ErrorCode::GraphicsInvalidFormat, "RenderGraph copy source and destination aspects differ");
    }
    if (ResourceDimension(source) != ResourceDimension(destination)) {
        return Err(ErrorCode::ValidationInvalidState, "RenderGraph copy source and destination dimensions differ");
    }
    if (ResourceSampleCount(source) != 1 || ResourceSampleCount(destination) != 1) {
        return Err(ErrorCode::ValidationInvalidState, "RenderGraph texture copies require single-sampled resources");
    }
    if (!ExtentsEqual(ResourceExtent(source, width, height), ResourceExtent(destination, width, height))) {
        return Err(ErrorCode::ValidationInvalidState, "RenderGraph whole-texture copy requires matching extents");
    }
    return Ok();
}

[[nodiscard]] Result<void> ValidateExtentMode(const ExtentMode& extent, const u32 width, const u32 height) {
    switch (extent.kind) {
        case ExtentModeKind::Swapchain:
            return Ok();
        case ExtentModeKind::Fixed:
            if (extent.width == 0 || extent.height == 0) {
                return Err(ErrorCode::ValidationOutOfRange, "RenderGraph fixed extent must be non-zero");
            }
            return Ok();
        case ExtentModeKind::Relative:
            if (!std::isfinite(extent.relative_width) || !std::isfinite(extent.relative_height) || extent.relative_width <= 0.f || extent.relative_height <= 0.f) {
                return Err(ErrorCode::ValidationOutOfRange, "RenderGraph relative extent must be finite and positive");
            }
            constexpr auto max = static_cast<double>(std::numeric_limits<u32>::max());
            if (static_cast<double>(width) * extent.relative_width > max || static_cast<double>(height) * extent.relative_height > max) {
                return Err(ErrorCode::ValidationOutOfRange, "RenderGraph relative extent exceeds the supported range");
            }
            return Ok();
    }
    return Err(ErrorCode::ValidationOutOfRange, "RenderGraph extent mode is invalid");
}

[[nodiscard]] Result<void> ValidateBlueprint(const GraphBlueprint& blueprint, const u32 width, const u32 height) {
    auto validate_resource_id = [&](const u32 id, const std::string& usage) -> Result<void> {
        if (id >= blueprint.resources.size()) {
            return Err(ErrorCode::ValidationOutOfRange, "RenderGraph " + usage + " references an invalid resource");
        }
        return Ok();
    };
    auto validate_attachment = [&](const u32 id, const bool depth, const std::string& usage) -> Result<void> {
        TRY_VOID(validate_resource_id(id, usage));
        const ResourceRecord& resource = blueprint.resources[id];
        const TextureFormat format = ResourceFormat(resource);
        if (format != TextureFormat::Undefined && IsDepthFormat(format) != depth) {
            return Err(ErrorCode::GraphicsInvalidFormat, "RenderGraph " + usage + " has an incompatible texture format");
        }
        if (resource.kind != ResourceKind::PerFrame && !HasFlag(ResourceUsage(resource), TextureUsage::RenderAttachment)) {
            return Err(ErrorCode::ValidationInvalidState, "RenderGraph " + usage + " requires RenderAttachment usage");
        }
        return Ok();
    };

    for (const ResourceRecord& resource : blueprint.resources) {
        if (resource.kind == ResourceKind::Owned && resource.owned_texture == nullptr) {
            return Err(ErrorCode::ValidationNullValue, "RenderGraph owned texture is null");
        }
        if (resource.kind == ResourceKind::Owned
            && (resource.owned_texture->GetFormat() == TextureFormat::Undefined || resource.owned_texture->GetWidth() == 0 || resource.owned_texture->GetHeight() == 0
                || resource.owned_texture->GetDepthOrArrayLayers() == 0)) {
            return Err(ErrorCode::ValidationInvalidState, "RenderGraph owned texture has an invalid format or extent");
        }
        if (resource.kind == ResourceKind::Transient) {
            if (resource.transient.format == TextureFormat::Undefined || resource.transient.usage == TextureUsage::None) {
                return Err(ErrorCode::ValidationInvalidState, "RenderGraph transient texture requires a format and usage");
            }
            TRY_VOID(ValidateExtentMode(resource.transient.extent, width, height));
        }
    }

    for (const FramebufferRecord& framebuffer : blueprint.framebuffers) {
        std::unordered_set<u32> slots{};
        for (const auto& [slot, resource_id] : framebuffer.colors) {
            if (!slots.insert(slot).second) {
                return Err(ErrorCode::ValidationInvalidState, "RenderGraph framebuffer has a duplicate color slot");
            }
            TRY_VOID(validate_attachment(resource_id, false, "framebuffer color attachment"));
        }
        if (framebuffer.depth_resource_id != kInvalidGraphResource) {
            TRY_VOID(validate_attachment(framebuffer.depth_resource_id, true, "framebuffer depth attachment"));
        }
    }

    std::unordered_set<std::string> pass_names{};
    for (const PassRecord& pass : blueprint.passes) {
        if (!pass_names.insert(pass.debug_name).second) {
            return Err(ErrorCode::ValidationInvalidState, "RenderGraph has duplicate pass name '" + pass.debug_name + "'");
        }

        const bool copy_pass = pass.kind == PassKind::Copy || !pass.copies.empty();
        if (copy_pass) {
            if (pass.copies.empty() || !pass.copy_execute) {
                return Err(ErrorCode::ValidationInvalidState, "RenderGraph copy pass '" + pass.debug_name + "' requires Copy operations and an Execute callback");
            }
            if (pass.render_execute || pass.framebuffer_id.has_value() || !pass.colors.empty() || pass.depth.has_value() || !pass.samples.empty()) {
                return Err(ErrorCode::ValidationInvalidState, "RenderGraph copy pass '" + pass.debug_name + "' mixes copy and render state");
            }
            for (const CopyOperation& copy : pass.copies) {
                TRY_VOID(validate_resource_id(copy.src_resource_id, "copy source"));
                TRY_VOID(validate_resource_id(copy.dst_resource_id, "copy destination"));
                const ResourceRecord& source = blueprint.resources[copy.src_resource_id];
                const ResourceRecord& destination = blueprint.resources[copy.dst_resource_id];
                if (source.kind == ResourceKind::PerFrame || destination.kind == ResourceKind::PerFrame) {
                    return Err(ErrorCode::ValidationInvalidState, "RenderGraph copy pass cannot use a per-frame view as a texture");
                }
                if (!HasFlag(ResourceUsage(source), TextureUsage::CopySrc) || !HasFlag(ResourceUsage(destination), TextureUsage::CopyDst)) {
                    return Err(ErrorCode::ValidationInvalidState, "RenderGraph copy resources require CopySrc and CopyDst usage");
                }
                TRY_VOID(ValidateCopyCompatibility(source, destination, width, height));
            }
            continue;
        }

        if (!pass.render_execute) {
            return Err(ErrorCode::ValidationInvalidState, "RenderGraph pass '" + pass.debug_name + "' has no Execute callback");
        }
        if (pass.copy_execute) {
            return Err(ErrorCode::ValidationInvalidState, "RenderGraph render pass '" + pass.debug_name + "' has a copy callback");
        }
        if (pass.framebuffer_id.has_value() && *pass.framebuffer_id >= blueprint.framebuffers.size()) {
            return Err(ErrorCode::ValidationOutOfRange, "RenderGraph pass '" + pass.debug_name + "' references an invalid framebuffer");
        }

        std::unordered_set<u32> color_slots{};
        std::unordered_set<u32> attachment_resources{};
        bool has_target = false;
        if (pass.framebuffer_id.has_value()) {
            const FramebufferRecord& framebuffer = blueprint.framebuffers[*pass.framebuffer_id];
            for (const auto& [slot, resource_id] : framebuffer.colors) {
                color_slots.insert(slot);
                attachment_resources.insert(resource_id);
                has_target = true;
            }
            has_target = has_target || framebuffer.depth_resource_id != kInvalidGraphResource;
            if (framebuffer.depth_resource_id != kInvalidGraphResource) {
                attachment_resources.insert(framebuffer.depth_resource_id);
            }
            if (pass.depth.has_value() && framebuffer.depth_resource_id != kInvalidGraphResource) {
                return Err(ErrorCode::ValidationInvalidState, "RenderGraph pass '" + pass.debug_name + "' has multiple depth attachments");
            }
        }
        for (const ColorOutput& color : pass.colors) {
            if (!color_slots.insert(color.slot).second) {
                return Err(ErrorCode::ValidationInvalidState, "RenderGraph pass '" + pass.debug_name + "' has a duplicate color slot");
            }
            TRY_VOID(validate_attachment(color.resource_id, false, "color attachment"));
            attachment_resources.insert(color.resource_id);
            has_target = true;
        }
        if (pass.depth.has_value()) {
            TRY_VOID(validate_attachment(pass.depth->resource_id, true, "depth attachment"));
            attachment_resources.insert(pass.depth->resource_id);
            has_target = true;
        }
        if (!has_target) {
            return Err(ErrorCode::ValidationInvalidState, "RenderGraph pass '" + pass.debug_name + "' has no render targets");
        }
        for (const SampleInput& sample : pass.samples) {
            TRY_VOID(validate_resource_id(sample.resource_id, "sample input"));
            const ResourceRecord& resource = blueprint.resources[sample.resource_id];
            if (attachment_resources.contains(sample.resource_id)) {
                return Err(ErrorCode::ValidationInvalidState, "RenderGraph pass cannot sample one of its attachments");
            }
            if (resource.kind == ResourceKind::PerFrame || !HasFlag(ResourceUsage(resource), TextureUsage::TextureBinding)) {
                return Err(ErrorCode::ValidationInvalidState, "RenderGraph sample input requires a texture with TextureBinding usage");
            }
            const TextureFormat format = ResourceFormat(resource);
            if (format != TextureFormat::Undefined && (sample.mode == SampleMode::DepthTexture) != IsDepthFormat(format)) {
                return Err(ErrorCode::GraphicsInvalidFormat, "RenderGraph sample mode is incompatible with the texture format");
            }
        }
    }
    return Ok();
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
    view_desc.usage = TextureUsage::TextureBinding;
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

    if (!factory) {
        return nullptr;
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
    std::unordered_set<u32> bindings{};
    for (const BindGroupEntryDesc& entry : entries_) {
        if (!bindings.insert(entry.binding).second) {
            return Err(ErrorCode::ValidationInvalidState, "BindGroupBuilder contains a duplicate binding");
        }
        if (entry.buffer != nullptr && entry.size == 0) {
            return Err(ErrorCode::ValidationOutOfRange, "BindGroupBuilder buffer binding size must be non-zero");
        }
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

        if (source->GetFormat() != destination->GetFormat() || ResourceAspects(source->GetFormat()) != ResourceAspects(destination->GetFormat())) {
            return Err(ErrorCode::GraphicsInvalidFormat, "CopyPassContext texture formats or aspects differ");
        }
        if (source->GetDimension() != destination->GetDimension() || source->GetSampleCount() != 1 || destination->GetSampleCount() != 1) {
            return Err(ErrorCode::ValidationInvalidState, "CopyPassContext textures require matching dimensions and single-sampled resources");
        }
        const Extent3D copy_size{source->GetWidth(), source->GetHeight(), source->GetDepthOrArrayLayers()};
        if (!ExtentsEqual(copy_size, Extent3D{destination->GetWidth(), destination->GetHeight(), destination->GetDepthOrArrayLayers()})) {
            return Err(ErrorCode::ValidationInvalidState, "CopyPassContext whole-texture copy requires matching extents");
        }
        if (copy_size.width == 0 || copy_size.height == 0 || copy_size.depth_or_array_layers == 0) {
            return Err(ErrorCode::ValidationOutOfRange, "CopyPassContext texture extent is empty");
        }
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

    TRY_VOID(ValidateBlueprint(*blueprint_, width, height));

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
    if (device == nullptr) {
        return Err(ErrorCode::ValidationNullValue, "RenderGraph requires a device");
    }
    if (width == 0 || height == 0) {
        return Err(ErrorCode::ValidationOutOfRange, "RenderGraph requires non-zero dimensions");
    }
    TRY_VOID(ValidateBlueprint(blueprint, width, height));

    auto graph = createRef<RenderGraph>(ConstructionKey{}, std::move(device), std::move(blueprint), width, height);
    if (graph == nullptr) {
        return Err(ErrorCode::GraphicsResourceCreationFailed, "Failed to allocate RenderGraph");
    }
    if (auto allocation = graph->AllocateRuntimeResources(width, height); !allocation) {
        return Err(std::move(allocation).error());
    }
    return Ok(std::move(graph));
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
}

Result<void> RenderGraph::AllocateRuntimeResources(const u32 width, const u32 height) {
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
                if (runtime.view == nullptr) {
                    return Err(ErrorCode::GraphicsResourceCreationFailed, "Failed to create RenderGraph owned texture view");
                }
            }
            if (IsDepthFormat(record.owned_texture->GetFormat()) && HasFlag(record.owned_texture->GetUsage(), TextureUsage::TextureBinding) && runtime.depth_sample_view == nullptr) {
                TextureViewDesc depth_view_desc{};
                depth_view_desc.label = "RenderGraphOwnedDepthSample";
                depth_view_desc.aspect = TextureAspect::DepthOnly;
                depth_view_desc.usage = TextureUsage::TextureBinding;
                runtime.depth_sample_view = record.owned_texture->CreateView(depth_view_desc);
                if (runtime.depth_sample_view == nullptr) {
                    return Err(ErrorCode::GraphicsResourceCreationFailed, "Failed to create RenderGraph owned depth sample view");
                }
            }
        }
    }

    width_ = width;
    height_ = height;
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
    if (*texture == nullptr) {
        return Err(ErrorCode::GraphicsTextureCreationFailed, "Device returned a null RenderGraph transient texture");
    }

    TextureViewDesc view_desc = MakeTransientViewDesc(record.transient);

    PooledTransientTexture pooled{};
    pooled.key = key;
    pooled.texture = std::move(*texture);
    pooled.view = pooled.texture->CreateView(view_desc);
    if (pooled.view == nullptr) {
        return Err(ErrorCode::GraphicsResourceCreationFailed, "Failed to create RenderGraph transient texture view");
    }
    if (IsDepthFormat(record.transient.format) && HasFlag(record.transient.usage, TextureUsage::TextureBinding)) {
        pooled.depth_sample_view = pooled.texture->CreateView(MakeDepthSampleViewDesc(record.transient));
        if (pooled.depth_sample_view == nullptr) {
            return Err(ErrorCode::GraphicsResourceCreationFailed, "Failed to create RenderGraph depth sample view");
        }
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
    if (active_frame_count_ != 0) {
        return Err(ErrorCode::InvalidState, "RenderGraph cannot resize while a frame is outstanding");
    }

    ReleaseTransientPool();
    if (auto allocation = AllocateRuntimeResources(width, height); !allocation) {
        ReleaseTransientPool();
        return allocation;
    }
    return Ok();
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
        if (runtime.blueprint.kind == ResourceKind::Owned && runtime.depth_sample_view != nullptr) {
            return runtime.depth_sample_view.get();
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

        const bool read_only = !pass.depth->config.write;
        depth_attachment = RenderPassDepthStencilAttachmentDesc{
            .view = depth_view,
            .depth_load_op = read_only ? LoadOp::Undefined : pass.depth->config.load,
            .depth_store_op = read_only ? StoreOp::Undefined : pass.depth->config.store,
            .depth_clear_value = read_only ? kDepthClearValueUndefined : pass.depth->config.clear,
            .depth_read_only = read_only,
            .stencil_load_op = LoadOp::Undefined,
            .stencil_store_op = StoreOp::Undefined,
            .stencil_read_only = true,
        };
    }

    if (color_attachments.empty() && !depth_attachment.has_value()) {
        return Err(ErrorCode::ValidationInvalidState, "RenderGraph pass '" + pass.debug_name + "' has no render targets");
    }

    std::vector<TextureView*> sample_views{};
    sample_views.reserve(pass.samples.size());
    for (const SampleInput& sample : pass.samples) {
        TextureView* view = ResolveSampleView(sample.resource_id, sample.mode);
        if (view == nullptr) {
            return Err(ErrorCode::GraphicsResourceCreationFailed, "RenderGraph pass '" + pass.debug_name + "' missing sample view");
        }
        sample_views.push_back(view);
    }

    RenderPassDescTyped pass_desc{};
    pass_desc.label = pass.debug_name;
    pass_desc.color_attachments = color_attachments;
    pass_desc.depth_stencil_attachment = depth_attachment.has_value() ? &*depth_attachment : nullptr;

    auto pass_encoder = encoder.BeginRenderPass(pass_desc);
    if (!pass_encoder) {
        return Err(pass_encoder.error());
    }
    if (*pass_encoder == nullptr) {
        return Err(ErrorCode::GraphicsResourceCreationFailed, "Command encoder returned a null render pass encoder");
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

    context.samples_ = std::move(sample_views);

    Result<void> execute = pass.render_execute(context);
    pass_encoder->get()->End();
    return execute;
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

    return pass.copy_execute(context);
}

Result<RenderGraphFrame> RenderGraph::BeginFrame(const u32 width, const u32 height) {
    if (device_ == nullptr) {
        return Err(ErrorCode::InvalidState, "RenderGraph has no device");
    }
    if (width == 0 || height == 0) {
        return Err(ErrorCode::ValidationOutOfRange, "RenderGraph frame dimensions must be non-zero");
    }
    if (width != width_ || height != height_) {
        TRY_VOID(RebuildForResize(width, height));
    }

    RenderGraphFrame frame(shared_from_this(), width, height);
    auto encoder = device_->CreateCommandEncoder({.label = "RenderGraphFrame"});
    if (!encoder) {
        return Err(std::move(encoder).error());
    }
    if (*encoder == nullptr) {
        return Err(ErrorCode::GraphicsResourceCreationFailed, "Device returned a null RenderGraph command encoder");
    }
    frame.encoder_ = std::move(*encoder);
    return Ok(std::move(frame));
}

// --- RenderGraphFrame ---

RenderGraphFrame::RenderGraphFrame(ref<RenderGraph> graph, const u32 width, const u32 height)
    : graph_(std::move(graph)),
      width_(width),
      height_(height) {
    ++graph_->active_frame_count_;
}

RenderGraphFrame::RenderGraphFrame(RenderGraphFrame&& other) noexcept
    : graph_(std::move(other.graph_)),
      width_(other.width_),
      height_(other.height_),
      encoder_(std::move(other.encoder_)),
      per_frame_views_(std::move(other.per_frame_views_)),
      executed_(other.executed_) {}

RenderGraphFrame& RenderGraphFrame::operator=(RenderGraphFrame&& other) noexcept {
    if (this != &other) {
        ReleaseFrame();
        graph_ = std::move(other.graph_);
        width_ = other.width_;
        height_ = other.height_;
        encoder_ = std::move(other.encoder_);
        per_frame_views_ = std::move(other.per_frame_views_);
        executed_ = other.executed_;
    }
    return *this;
}

RenderGraphFrame::~RenderGraphFrame() {
    ReleaseFrame();
}

void RenderGraphFrame::ReleaseFrame() noexcept {
    if (graph_ != nullptr) {
        WOKI_ASSERT(graph_->active_frame_count_ > 0);
        --graph_->active_frame_count_;
        graph_.reset();
    }
}

void RenderGraphFrame::Bind(const PerFrameSlot slot, ref<TextureView> view) {
    if (!slot || graph_ == nullptr) {
        return;
    }

    per_frame_views_[slot.id_] = std::move(view);
}

Result<void> RenderGraphFrame::Execute() {
    if (executed_) {
        return Err(ErrorCode::InvalidState, "RenderGraphFrame has already been executed");
    }
    if (graph_ == nullptr || graph_->device_ == nullptr || !encoder_) {
        return Err(ErrorCode::InvalidState, "RenderGraphFrame is invalid");
    }
    executed_ = true;

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
    if (*command_buffer == nullptr) {
        return Err(ErrorCode::GraphicsResourceCreationFailed, "Command encoder returned a null command buffer");
    }

    CommandBuffer* buffers[] = {command_buffer->get()};
    encoder_.reset();
    return graph_->device_->GetQueue().Submit(buffers);
}

} // namespace woki::rhi
