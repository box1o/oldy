#pragma once

#include <string>
#include <vector>
#include <utility>
#include <optional>
#include <unordered_map>

#include <woki/core.hpp>
#include <woki/rhi/forward.hpp>

#include "context.hpp"
#include "resources.hpp"

namespace woki::rhi::render_graph::detail {

enum class ResourceKind : u8 {
    Transient,
    Owned,
    PerFrame,
};

struct ResourceRecord final {
    ResourceKind kind{ResourceKind::Transient};
    TransientDesc transient{};
    ref<Texture> owned_texture{};
};

struct FramebufferRecord final {
    std::vector<std::pair<u32, u32>> colors{};
    u32 depth_resource_id{kInvalidGraphResource};
};

struct ColorOutput final {
    u32 slot{0};
    u32 resource_id{kInvalidGraphResource};
    ColorAttachmentConfig config{};
};

struct DepthOutput final {
    u32 resource_id{kInvalidGraphResource};
    DepthAttachmentConfig config{};
};

struct SampleInput final {
    u32 resource_id{kInvalidGraphResource};
    SampleMode mode{SampleMode::ColorTexture};
};

enum class PassKind : u8 {
    Render,
    Copy,
};

struct CopyOperation final {
    u32 src_resource_id{kInvalidGraphResource};
    u32 dst_resource_id{kInvalidGraphResource};
};

struct PassRecord final {
    std::string debug_name{};
    PassKind kind{PassKind::Render};
    std::optional<u32> framebuffer_id{};
    FramebufferTargetConfig framebuffer_config{};
    std::vector<ColorOutput> colors{};
    std::optional<DepthOutput> depth{};
    std::vector<SampleInput> samples{};
    std::vector<CopyOperation> copies{};
    std::function<void(RenderPassContext&)> render_execute{};
    std::function<void(CopyPassContext&)> copy_execute{};
    ref<void> user_data{};
};

struct TransientPoolKey final {
    TextureFormat format{TextureFormat::Undefined};
    TextureUsage usage{};
    u32 width{0};
    u32 height{0};

    [[nodiscard]] bool operator==(const TransientPoolKey&) const = default;
};

struct PooledTransientTexture final {
    TransientPoolKey key{};
    scope<Texture> texture{};
    scope<TextureView> view{};
    scope<TextureView> depth_sample_view{};
    bool in_use{false};
};

struct GraphBlueprint final {
    std::vector<ResourceRecord> resources{};
    std::vector<FramebufferRecord> framebuffers{};
    std::vector<PassRecord> passes{};
    std::unordered_map<std::string, u32> pass_name_to_index{};
};

} // namespace woki::rhi::render_graph::detail
