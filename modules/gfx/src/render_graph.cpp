#include <cmath>

#include "internal/graph.hpp"

namespace woki::gfx {
namespace {

template <typename Handle>
Handle MakeHandle(const std::shared_ptr<graph_detail::Definition>& definition, u32 index) {
    return Handle::FromGraph(index, definition->identity->generation, definition->identity);
}

template <typename Handle>
bool Valid(const std::shared_ptr<graph_detail::Definition>& definition, const Handle& handle) {
    return definition != nullptr && definition->identity->open && graph_detail::Belongs(*definition, handle);
}

bool MutablePass(const std::shared_ptr<graph_detail::Definition>& definition, const GraphPass& pass) {
    return Valid(definition, pass) && pass.Index() < definition->passes.size();
}

u32 AddVersion(const std::shared_ptr<graph_detail::Definition>& definition, u32 resource, u32 producer) {
    auto& value = definition->resources[resource];
    const u32 previous = value.current_version;
    const u32 ordinal = previous == kInvalidGraphIndex ? 0 : definition->versions[previous].ordinal + 1;
    const u32 index = static_cast<u32>(definition->versions.size());
    definition->versions.push_back(
        {.type = value.type, .resource = resource, .ordinal = ordinal, .producer = producer, .previous = previous}
    );
    value.current_version = index;
    if (value.initial_version == kInvalidGraphIndex)
        value.initial_version = index;
    return index;
}

} // namespace

namespace graph_detail {

rhi::Extent3D ResolveExtent(const GraphTextureDesc& descriptor, const u32 width, const u32 height) {
    if (descriptor.extent.kind == ExtentKind::Fixed)
        return {descriptor.extent.width, descriptor.extent.height, descriptor.depth_or_layers};
    return {std::max(1u, static_cast<u32>(static_cast<f32>(width) * descriptor.extent.scale_x)),
        std::max(1u, static_cast<u32>(static_cast<f32>(height) * descriptor.extent.scale_y)),
        descriptor.depth_or_layers};
}

bool IsDepthFormat(const rhi::TextureFormat format) noexcept {
    switch (format) {
        case rhi::TextureFormat::Depth16Unorm:
        case rhi::TextureFormat::Depth24Plus:
        case rhi::TextureFormat::Depth24PlusStencil8:
        case rhi::TextureFormat::Depth32Float:
        case rhi::TextureFormat::Depth32FloatStencil8:
            return true;
        default:
            return false;
    }
}

u64 TextureBytes(const GraphTextureDesc& descriptor, const rhi::Extent3D& extent) noexcept {
    u64 bytes_per_texel = 4;
    switch (descriptor.format) {
        case rhi::TextureFormat::R8Unorm:
            bytes_per_texel = 1;
            break;
        case rhi::TextureFormat::RG8Unorm:
        case rhi::TextureFormat::R16Float:
        case rhi::TextureFormat::Depth16Unorm:
            bytes_per_texel = 2;
            break;
        case rhi::TextureFormat::RGBA16Float:
            bytes_per_texel = 8;
            break;
        case rhi::TextureFormat::RGBA32Float:
            bytes_per_texel = 16;
            break;
        default:
            break;
    }
    u64 total{};
    u64 w = extent.width;
    u64 h = extent.height;
    for (u32 mip = 0; mip < descriptor.mip_levels; ++mip) {
        total += std::max<u64>(1, w) * std::max<u64>(1, h) * extent.depth_or_array_layers * bytes_per_texel
                 * descriptor.sample_count;
        w = std::max<u64>(1, w / 2);
        h = std::max<u64>(1, h / 2);
    }
    return total;
}

rhi::TextureUsage TextureUsageFor(const GraphAccess access) noexcept {
    switch (access) {
        case GraphAccess::Sampled:
            return rhi::TextureUsage::TextureBinding;
        case GraphAccess::StorageRead:
        case GraphAccess::StorageWrite:
            return rhi::TextureUsage::StorageBinding;
        case GraphAccess::ColorAttachment:
        case GraphAccess::DepthRead:
        case GraphAccess::DepthWrite:
            return rhi::TextureUsage::RenderAttachment;
        case GraphAccess::CopySource:
            return rhi::TextureUsage::CopySrc;
        case GraphAccess::CopyDestination:
            return rhi::TextureUsage::CopyDst;
        default:
            return rhi::TextureUsage::None;
    }
}

rhi::BufferUsage BufferUsageFor(const GraphAccess access) noexcept {
    switch (access) {
        case GraphAccess::StorageRead:
        case GraphAccess::StorageWrite:
            return rhi::BufferUsage::Storage;
        case GraphAccess::CopySource:
            return rhi::BufferUsage::CopySrc;
        case GraphAccess::CopyDestination:
            return rhi::BufferUsage::CopyDst;
        case GraphAccess::Vertex:
            return rhi::BufferUsage::Vertex;
        case GraphAccess::Index:
            return rhi::BufferUsage::Index;
        case GraphAccess::Uniform:
            return rhi::BufferUsage::Uniform;
        case GraphAccess::Indirect:
            return rhi::BufferUsage::Indirect;
        default:
            return rhi::BufferUsage::None;
    }
}

bool IsRead(const GraphAccess access) noexcept {
    return access == GraphAccess::Sampled || access == GraphAccess::StorageRead || access == GraphAccess::DepthRead
           || access == GraphAccess::CopySource || access == GraphAccess::Vertex || access == GraphAccess::Index
           || access == GraphAccess::Uniform || access == GraphAccess::Indirect;
}

bool IsWrite(const GraphAccess access) noexcept {
    return access == GraphAccess::StorageWrite || access == GraphAccess::ColorAttachment
           || access == GraphAccess::DepthWrite || access == GraphAccess::CopyDestination;
}

TextureSubresourceRange NormalizeRange(const GraphTextureDesc& descriptor, TextureSubresourceRange range) noexcept {
    if (range.aspect == rhi::TextureAspect::Undefined)
        range.aspect = descriptor.view_aspect;
    if (range.mip_level_count == kRemainingSubresources && range.base_mip_level <= descriptor.mip_levels)
        range.mip_level_count = descriptor.mip_levels - range.base_mip_level;
    if (range.array_layer_count == kRemainingSubresources && range.base_array_layer <= descriptor.depth_or_layers)
        range.array_layer_count = descriptor.depth_or_layers - range.base_array_layer;
    return range;
}

bool Overlaps(const GraphTextureDesc& descriptor, TextureSubresourceRange lhs, TextureSubresourceRange rhs) noexcept {
    lhs = NormalizeRange(descriptor, lhs);
    rhs = NormalizeRange(descriptor, rhs);
    const bool aspects = lhs.aspect == rhi::TextureAspect::All || rhs.aspect == rhi::TextureAspect::All
                         || lhs.aspect == rhs.aspect;
    const bool mips = lhs.base_mip_level < rhs.base_mip_level + rhs.mip_level_count
                      && rhs.base_mip_level < lhs.base_mip_level + lhs.mip_level_count;
    const bool layers = lhs.base_array_layer < rhs.base_array_layer + rhs.array_layer_count
                        && rhs.base_array_layer < lhs.base_array_layer + lhs.array_layer_count;
    return aspects && mips && layers;
}

rhi::Extent3D SubresourceExtent(const rhi::Extent3D& extent, const TextureSubresourceRange range) noexcept {
    return {std::max(1U, extent.width >> range.base_mip_level),
        std::max(1U, extent.height >> range.base_mip_level),
        range.array_layer_count};
}

} // namespace graph_detail

PassBuilder::PassBuilder(std::shared_ptr<graph_detail::Definition> definition, const GraphPass pass)
    : definition_(std::move(definition)),
      pass_(pass) {}

PassBuilder& PassBuilder::Read(
    const GraphTextureRef resource,
    const GraphAccess access,
    TextureSubresourceRange range,
    GraphTextureViewDesc view
) {
    if (!MutablePass(definition_, pass_))
        return *this;
    if (Valid(definition_, resource) && resource.index_ < definition_->versions.size())
        definition_->passes[pass_.index_].uses.push_back({resource.index_, access, false, range, std::move(view)});
    else
        definition_->passes[pass_.index_].uses.push_back({kInvalidGraphIndex, access, false, range, std::move(view)});
    return *this;
}

PassBuilder& PassBuilder::Read(const GraphBufferRef resource, const GraphAccess access) {
    if (!MutablePass(definition_, pass_))
        return *this;
    if (Valid(definition_, resource) && resource.index_ < definition_->versions.size())
        definition_->passes[pass_.index_]
            .uses
            .push_back({.version = resource.index_, .access = access, .write = false, .subresources = {}, .view = {}});
    else
        definition_->passes[pass_.index_].uses.push_back(
            {.version = kInvalidGraphIndex, .access = access, .write = false, .subresources = {}, .view = {}}
        );
    return *this;
}

GraphTextureRef PassBuilder::Write(
    const GraphTexture resource,
    const GraphAccess access,
    TextureSubresourceRange range,
    GraphTextureViewDesc view
) {
    if (!MutablePass(definition_, pass_) || !Valid(definition_, resource)
        || resource.index_ >= definition_->resources.size())
        return {};
    const u32 version = AddVersion(definition_, resource.index_, pass_.index_);
    definition_->passes[pass_.index_].uses.push_back({version, access, true, range, std::move(view)});
    return MakeHandle<GraphTextureRef>(definition_, version);
}

GraphBufferRef PassBuilder::Write(const GraphBuffer resource, const GraphAccess access) {
    if (!MutablePass(definition_, pass_) || !Valid(definition_, resource)
        || resource.index_ >= definition_->resources.size())
        return {};
    const u32 version = AddVersion(definition_, resource.index_, pass_.index_);
    definition_->passes[pass_.index_]
        .uses.push_back({.version = version, .access = access, .write = true, .subresources = {}, .view = {}});
    return MakeHandle<GraphBufferRef>(definition_, version);
}

GraphTextureRef PassBuilder::ReadWrite(const GraphTextureRef resource, const GraphAccess access) {
    if (!MutablePass(definition_, pass_) || !Valid(definition_, resource)
        || resource.index_ >= definition_->versions.size())
        return {};
    Read(resource, GraphAccess::StorageRead);
    const auto logical = definition_->versions[resource.index_].resource;
    auto output = Write(MakeHandle<GraphTexture>(definition_, logical), access);
    if (output)
        definition_->passes[pass_.index_].approved_read_writes.emplace(resource.index_, output.index_);
    return output;
}

GraphBufferRef PassBuilder::ReadWrite(const GraphBufferRef resource, const GraphAccess access) {
    if (!MutablePass(definition_, pass_) || !Valid(definition_, resource)
        || resource.index_ >= definition_->versions.size())
        return {};
    Read(resource, GraphAccess::StorageRead);
    const auto logical = definition_->versions[resource.index_].resource;
    auto output = Write(MakeHandle<GraphBuffer>(definition_, logical), access);
    if (output)
        definition_->passes[pass_.index_].approved_read_writes.emplace(resource.index_, output.index_);
    return output;
}

GraphTextureRef PassBuilder::Color(const GraphTexture resource, ColorAttachment attachment) {
    auto output = Write(
        resource,
        GraphAccess::ColorAttachment,
        attachment.region.view.subresources,
        attachment.region.view
    );
    if (output) {
        const u32 previous = definition_->versions[output.index_].previous;
        if (attachment.load == rhi::LoadOp::Load && previous != kInvalidGraphIndex) {
            Read(
                MakeHandle<GraphTextureRef>(definition_, previous),
                GraphAccess::ColorAttachment,
                attachment.region.view.subresources,
                attachment.region.view
            );
            definition_->passes[pass_.index_].approved_read_writes.emplace(previous, output.index_);
        }
        definition_->passes[pass_.index_].colors.push_back({output.index_, std::move(attachment)});
    }
    return output;
}

GraphTextureRef PassBuilder::ResolveColor(
    const GraphTexture multisample,
    const GraphTexture resolve,
    ColorAttachment attachment
) {
    auto color = Color(multisample, std::move(attachment));
    auto resolved = Write(resolve, GraphAccess::ColorAttachment);
    if (color && resolved)
        definition_->passes[pass_.index_].colors.back().attachment.resolve = resolved;
    return resolved;
}

GraphTextureRef PassBuilder::Depth(const GraphTexture resource, DepthAttachment attachment) {
    GraphTextureRef output;
    if (!MutablePass(definition_, pass_) || !Valid(definition_, resource)
        || resource.index_ >= definition_->resources.size())
        return output;
    if (attachment.read_only) {
        const auto current = definition_->resources[resource.index_].current_version;
        if (current == kInvalidGraphIndex)
            return output;
        output = MakeHandle<GraphTextureRef>(definition_, current);
        Read(output, GraphAccess::DepthRead);
    } else {
        output = Write(resource, GraphAccess::DepthWrite, attachment.region.view.subresources, attachment.region.view);
        const u32 previous = output ? definition_->versions[output.index_].previous : kInvalidGraphIndex;
        if (output && attachment.load == rhi::LoadOp::Load && previous != kInvalidGraphIndex) {
            Read(
                MakeHandle<GraphTextureRef>(definition_, previous),
                GraphAccess::DepthRead,
                attachment.region.view.subresources,
                attachment.region.view
            );
            definition_->passes[pass_.index_].approved_read_writes.emplace(previous, output.index_);
        }
    }
    if (output)
        definition_->passes[pass_.index_].depth = graph_detail::Depth{output.index_, std::move(attachment)};
    return output;
}

PassBuilder& PassBuilder::Copy(const GraphTextureRef source, const GraphTexture destination) {
    if (!MutablePass(definition_, pass_))
        return *this;
    Read(source, GraphAccess::CopySource);
    auto output = Write(destination, GraphAccess::CopyDestination);
    definition_->passes[pass_.index_].copies.push_back({source.index_, output.index_, 0});
    return *this;
}

PassBuilder& PassBuilder::Copy(const GraphBufferRef source, const GraphBuffer destination, const u64 size) {
    if (!MutablePass(definition_, pass_))
        return *this;
    Read(source, GraphAccess::CopySource);
    auto output = Write(destination, GraphAccess::CopyDestination);
    definition_->passes[pass_.index_].copies.push_back({source.index_, output.index_, size});
    return *this;
}

PassBuilder& PassBuilder::DependsOn(const GraphPass pass) {
    if (!MutablePass(definition_, pass_))
        return *this;
    definition_->passes[pass_.index_]
        .explicit_dependencies.push_back(Valid(definition_, pass) ? pass.index_ : kInvalidGraphIndex);
    return *this;
}

PassBuilder& PassBuilder::Queue(const QueuePreference preference, const bool async_compute_eligible) {
    if (!MutablePass(definition_, pass_))
        return *this;
    auto& pass = definition_->passes[pass_.index_];
    pass.preference = preference;
    pass.async_compute = async_compute_eligible;
    return *this;
}

PassBuilder& PassBuilder::SideEffect(std::string name) {
    if (!MutablePass(definition_, pass_))
        return *this;
    definition_->passes[pass_.index_].side_effect = std::move(name);
    return *this;
}

PassBuilder& PassBuilder::Execute(GraphExecuteCallback callback) {
    if (!MutablePass(definition_, pass_))
        return *this;
    definition_->passes[pass_.index_].callback = std::move(callback);
    return *this;
}

RenderGraphBuilder::RenderGraphBuilder()
    : definition_(std::make_shared<graph_detail::Definition>()) {}

GraphTexture RenderGraphBuilder::CreateTexture(GraphTextureDesc descriptor, const ResourceLifetime lifetime) {
    if (definition_ == nullptr || !definition_->identity->open)
        return {};
    const u32 index = static_cast<u32>(definition_->resources.size());
    graph_detail::Resource resource;
    resource.type = graph_detail::ResourceType::Texture;
    resource.lifetime = lifetime;
    resource.texture = std::move(descriptor);
    definition_->resources.push_back(std::move(resource));
    AddVersion(definition_, index, kInvalidGraphIndex);
    return MakeHandle<GraphTexture>(definition_, index);
}

GraphBuffer RenderGraphBuilder::CreateBuffer(GraphBufferDesc descriptor, const ResourceLifetime lifetime) {
    if (definition_ == nullptr || !definition_->identity->open)
        return {};
    const u32 index = static_cast<u32>(definition_->resources.size());
    graph_detail::Resource resource;
    resource.type = graph_detail::ResourceType::Buffer;
    resource.lifetime = lifetime;
    resource.buffer = std::move(descriptor);
    definition_->resources.push_back(std::move(resource));
    AddVersion(definition_, index, kInvalidGraphIndex);
    return MakeHandle<GraphBuffer>(definition_, index);
}

GraphTexture RenderGraphBuilder::ImportTexture(ExternalTextureContract contract, ref<rhi::Texture> texture) {
    if (definition_ == nullptr || !definition_->identity->open)
        return {};
    const ResourceLifetime lifetime = contract.frame_bound ? (contract.final_state == ExternalState::Present
                                                                     ? ResourceLifetime::Presentation
                                                                     : ResourceLifetime::Imported)
                                                           : ResourceLifetime::Imported;
    auto result = CreateTexture(std::move(contract.descriptor), lifetime);
    auto& resource = definition_->resources[result.index_];
    resource.initial_state = contract.initial_state;
    resource.default_view = std::move(contract.default_view);
    resource.final_state = contract.final_state;
    resource.frame_bound = contract.frame_bound;
    resource.external = true;
    resource.imported_texture = std::move(texture);
    return result;
}

GraphBuffer RenderGraphBuilder::ImportBuffer(ExternalBufferContract contract, ref<rhi::Buffer> buffer) {
    if (definition_ == nullptr || !definition_->identity->open)
        return {};
    auto result = CreateBuffer(
        std::move(contract.descriptor),
        contract.final_state == ExternalState::HostRead ? ResourceLifetime::Readback : ResourceLifetime::Imported
    );
    auto& resource = definition_->resources[result.index_];
    resource.initial_state = contract.initial_state;
    resource.final_state = contract.final_state;
    resource.frame_bound = contract.frame_bound;
    resource.external = true;
    resource.imported_buffer = std::move(buffer);
    return result;
}

GraphTemporalTexture RenderGraphBuilder::ImportTemporal(const TemporalTextureImport& history) {
    if (definition_ == nullptr || !definition_->identity->open)
        return {};
    ExternalTextureContract previous_contract;
    previous_contract.descriptor = history.descriptor;
    previous_contract.initial_state = ExternalState::ShaderRead;
    previous_contract.final_state = ExternalState::ShaderRead;
    previous_contract.frame_bound = history.previous == nullptr;
    auto previous = ImportTexture(std::move(previous_contract), history.previous);
    definition_->resources[previous.index_].lifetime = ResourceLifetime::Temporal;

    ExternalTextureContract current_contract;
    current_contract.descriptor = history.descriptor;
    current_contract.initial_state = ExternalState::ShaderRead;
    current_contract.final_state = ExternalState::ShaderRead;
    current_contract.frame_bound = history.current == nullptr;
    auto current = ImportTexture(std::move(current_contract), history.current);
    definition_->resources[current.index_].lifetime = ResourceLifetime::Temporal;
    definition_->resources[previous.index_].temporal_peer = current.index_;
    definition_->resources[current.index_].temporal_peer = previous.index_;
    return {.previous = previous, .previous_version = Initial(previous), .current = current};
}

GraphTextureRef RenderGraphBuilder::Initial(const GraphTexture resource) const {
    if (!Valid(definition_, resource) || resource.index_ >= definition_->resources.size())
        return {};
    return MakeHandle<GraphTextureRef>(definition_, definition_->resources[resource.index_].initial_version);
}

GraphBufferRef RenderGraphBuilder::Initial(const GraphBuffer resource) const {
    if (!Valid(definition_, resource) || resource.index_ >= definition_->resources.size())
        return {};
    return MakeHandle<GraphBufferRef>(definition_, definition_->resources[resource.index_].initial_version);
}

PassBuilder RenderGraphBuilder::AddPass(std::string name, const PassKind kind) {
    if (definition_ == nullptr || !definition_->identity->open)
        return PassBuilder({}, {});
    const u32 index = static_cast<u32>(definition_->passes.size());
    graph_detail::Pass pass;
    pass.name = std::move(name);
    pass.kind = kind;
    definition_->passes.push_back(std::move(pass));
    return PassBuilder(definition_, MakeHandle<GraphPass>(definition_, index));
}

Result<void> RenderGraphBuilder::Export(const GraphTextureRef resource, const ExternalState final_state) {
    if (!Valid(definition_, resource) || resource.index_ >= definition_->versions.size())
        return Err(ErrorCode::ValidationInvalidState, "GRF1002 stale or cross-graph texture version");
    definition_->exports.insert(resource.index_);
    definition_->resources[definition_->versions[resource.index_].resource].final_state = final_state;
    return Ok();
}

Result<void> RenderGraphBuilder::Export(const GraphBufferRef resource, const ExternalState final_state) {
    if (!Valid(definition_, resource) || resource.index_ >= definition_->versions.size())
        return Err(ErrorCode::ValidationInvalidState, "GRF1002 stale or cross-graph buffer version");
    definition_->exports.insert(resource.index_);
    definition_->resources[definition_->versions[resource.index_].resource].final_state = final_state;
    return Ok();
}

GraphBlackboard& RenderGraphBuilder::Blackboard() noexcept {
    if (definition_ == nullptr || !definition_->identity->open)
        return closed_blackboard_;
    return definition_->blackboard;
}

Result<CompiledRenderGraph> RenderGraphBuilder::Compile(const u32 width, const u32 height) {
    auto report = CompileWithReport(width, height);
    if (report.graph)
        return Ok(std::move(*report.graph));
    if (report.diagnostics.empty())
        return Err(ErrorCode::InvalidState, "GRF1001 graph builder is closed");
    const auto& diagnostic = report.diagnostics.front();
    return Err(ErrorCode::ValidationInvalidState, diagnostic.code + " " + diagnostic.message);
}

GraphCompileReport RenderGraphBuilder::CompileWithReport(const u32 width, const u32 height) {
    if (definition_ == nullptr || !definition_->identity->open)
        return {.graph = std::nullopt,
            .diagnostics = {{.code = "GRF1001",
                .message = "graph builder is closed",
                .stage = "builder",
                .pass = {},
                .resource = {},
                .chain = {}}}};
    auto detached = std::make_shared<graph_detail::Definition>();
    detached->identity = std::move(definition_->identity);
    detached->identity->open = false;
    detached->resources = std::move(definition_->resources);
    detached->versions = std::move(definition_->versions);
    detached->passes = std::move(definition_->passes);
    detached->exports = std::move(definition_->exports);
    detached->blackboard = definition_->blackboard;
    definition_->identity = std::make_shared<graph_detail::Identity>();
    definition_->identity->generation = detached->identity->generation + 1;
    definition_->identity->open = false;
    definition_.reset();
    return GraphCompiler::CompileWithReport(std::move(detached), width, height);
}

} // namespace woki::gfx
