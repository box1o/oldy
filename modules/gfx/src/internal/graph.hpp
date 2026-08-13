#pragma once

#include <map>
#include <set>

#include <woki/gfx/advanced/graph_compile.hpp>

namespace woki::gfx::graph_detail {

struct Identity final {
    u32 generation{1};
    bool open{true};
};

enum class ResourceType : u8 { Texture, Buffer };

struct Resource final {
    ResourceType type{ResourceType::Texture};
    ResourceLifetime lifetime{ResourceLifetime::Transient};
    GraphTextureDesc texture;
    GraphTextureViewDesc default_view;
    GraphBufferDesc buffer;
    ExternalState initial_state{ExternalState::Undefined};
    ExternalState final_state{ExternalState::Undefined};
    bool frame_bound{};
    bool external{};
    ref<rhi::Texture> imported_texture;
    ref<rhi::Buffer> imported_buffer;
    u32 temporal_peer{kInvalidGraphIndex};
    u32 initial_version{kInvalidGraphIndex};
    u32 current_version{kInvalidGraphIndex};
};

struct Version final {
    ResourceType type{ResourceType::Texture};
    u32 resource{};
    u32 ordinal{};
    u32 producer{kInvalidGraphIndex};
    u32 previous{kInvalidGraphIndex};
};

struct Use final {
    u32 version{};
    GraphAccess access{GraphAccess::Sampled};
    bool write{};
    TextureSubresourceRange subresources;
    GraphTextureViewDesc view;
};

struct Color final {
    u32 version{};
    ColorAttachment attachment;
};

struct Depth final {
    u32 version{};
    DepthAttachment attachment;
};

struct Copy final {
    u32 source{};
    u32 destination{};
    u64 size{};
};

struct Pass final {
    std::string name;
    PassKind kind{PassKind::Render};
    QueuePreference preference{QueuePreference::Automatic};
    bool async_compute{};
    std::optional<std::string> side_effect;
    std::vector<Use> uses;
    std::set<std::pair<u32, u32>> approved_read_writes;
    std::vector<Color> colors;
    std::optional<Depth> depth;
    std::vector<Copy> copies;
    std::vector<u32> explicit_dependencies;
    GraphExecuteCallback callback;
};

struct Definition final {
    std::shared_ptr<Identity> identity{std::make_shared<Identity>()};
    std::vector<Resource> resources;
    std::vector<Version> versions;
    std::vector<Pass> passes;
    std::set<u32> exports;
    GraphBlackboard blackboard;
};

template <typename Handle>
[[nodiscard]] bool Belongs(const Definition& definition, const Handle& handle) {
    return handle.IsFrom(definition.identity.get()) && handle.Generation() == definition.identity->generation;
}

[[nodiscard]] rhi::Extent3D ResolveExtent(const GraphTextureDesc& descriptor, u32 width, u32 height);
[[nodiscard]] bool IsDepthFormat(rhi::TextureFormat format) noexcept;
[[nodiscard]] u64 TextureBytes(const GraphTextureDesc& descriptor, const rhi::Extent3D& extent) noexcept;
[[nodiscard]] rhi::TextureUsage TextureUsageFor(GraphAccess access) noexcept;
[[nodiscard]] rhi::BufferUsage BufferUsageFor(GraphAccess access) noexcept;
[[nodiscard]] bool IsRead(GraphAccess access) noexcept;
[[nodiscard]] bool IsWrite(GraphAccess access) noexcept;
[[nodiscard]] TextureSubresourceRange NormalizeRange(const GraphTextureDesc& descriptor, TextureSubresourceRange range) noexcept;
[[nodiscard]] bool Overlaps(const GraphTextureDesc& descriptor, TextureSubresourceRange lhs, TextureSubresourceRange rhs) noexcept;
[[nodiscard]] rhi::Extent3D SubresourceExtent(const rhi::Extent3D& extent, TextureSubresourceRange range) noexcept;

} // namespace woki::gfx::graph_detail

namespace woki::gfx {

struct CompiledRenderGraph::Impl final {
    std::shared_ptr<graph_detail::Definition> definition;
    u32 width{};
    u32 height{};
    std::vector<CompiledPassInfo> passes;
    std::vector<GraphDependency> dependencies;
    std::vector<GraphLifetimeInterval> lifetimes;
    std::vector<GraphTransition> transitions;
    std::vector<std::vector<u32>> waves;
    std::vector<GraphDiagnostic> diagnostics;
    std::vector<u32> schedule;
    std::vector<bool> retained_passes;
    std::vector<bool> retained_resources;
    std::vector<u32> resource_slots;
    std::vector<rhi::Extent3D> extents;
    u32 texture_slot_count{};
    u32 buffer_slot_count{};
    u64 peak_bytes{};
    u64 hash{};
};

} // namespace woki::gfx
