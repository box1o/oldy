#pragma once

#include <string>
#include <vector>
#include <string_view>

#include <woki/enums.hpp>
#include <woki/rhi/descriptors.hpp>

namespace woki::rhi {

class RenderGraphBuilder;

inline constexpr u32 kInvalidGraphResource = 0xFFFFFFFFu;

enum class SampleMode : u8 {
    ColorTexture,
    DepthTexture,
};

enum class ExtentModeKind : u8 {
    Swapchain,
    Fixed,
    Relative,
};

struct ExtentMode final {
    ExtentModeKind kind{ExtentModeKind::Swapchain};
    u32 width{0};
    u32 height{0};
    f32 relative_width{1.f};
    f32 relative_height{1.f};

    [[nodiscard]] static ExtentMode Swapchain() noexcept {
        return ExtentMode{.kind = ExtentModeKind::Swapchain};
    }

    [[nodiscard]] static ExtentMode Fixed(const u32 width, const u32 height) noexcept {
        return ExtentMode{.kind = ExtentModeKind::Fixed, .width = width, .height = height};
    }

    [[nodiscard]] static ExtentMode Relative(const f32 width, const f32 height) noexcept {
        return ExtentMode{
            .kind = ExtentModeKind::Relative,
            .relative_width = width,
            .relative_height = height,
        };
    }
};

struct TransientDesc final {
    std::string label{};
    TextureFormat format{TextureFormat::Undefined};
    TextureUsage usage{};
    ExtentMode extent{ExtentMode::Swapchain()};
};

struct ColorAttachmentConfig final {
    LoadOp load{LoadOp::Clear};
    StoreOp store{StoreOp::Store};
    Color clear{0.12, 0.12, 0.18, 1.0};
};

struct DepthAttachmentConfig final {
    LoadOp load{LoadOp::Clear};
    StoreOp store{StoreOp::Store};
    f32 clear{1.f};
    bool write{true};
};

struct FramebufferTargetConfig final {
    std::vector<Color> clear_color{};
    f32 clear_depth{1.f};
};

class Resource final {
public:
    Resource() = default;

    [[nodiscard]] explicit operator bool() const noexcept {
        return id_ != kInvalidGraphResource;
    }

private:
    friend class RenderGraphBuilder;
    friend class PassBuilder;
    friend class FramebufferBuilder;
    u32 id_{kInvalidGraphResource};
};

class PerFrameSlot final {
public:
    PerFrameSlot() = default;

    [[nodiscard]] explicit operator bool() const noexcept {
        return id_ != kInvalidGraphResource;
    }

private:
    friend class RenderGraphBuilder;
    friend class RenderGraphFrame;
    friend class PassBuilder;
    u32 id_{kInvalidGraphResource};
};

class Framebuffer final {
public:
    Framebuffer() = default;

    [[nodiscard]] explicit operator bool() const noexcept {
        return id_ != kInvalidGraphResource;
    }

private:
    friend class RenderGraphBuilder;
    friend class PassBuilder;
    friend class FramebufferBuilder;
    u32 id_{kInvalidGraphResource};
};

} // namespace woki::rhi
