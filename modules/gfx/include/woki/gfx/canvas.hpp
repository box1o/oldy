#pragma once

#include <span>
#include <compare>
#include <variant>
#include <vector>

#include "handles.hpp"
#include "presentation.hpp"

namespace woki::gfx {

struct CanvasRect final {
    f32 x{}, y{}, width{}, height{};
    friend bool operator==(const CanvasRect&, const CanvasRect&) = default;
};

struct CanvasVertex final {
    f32 x{}, y{};
    f32 u{}, v{};
    f32 r{}, g{}, b{}, a{};
    f32 p0{}, p1{}, p2{}, p3{};
    f32 q0{}, q1{};
};

using CanvasImageSource = std::variant<std::monostate, TextureHandle, OffscreenTargetHandle>;

enum class CanvasPrimitive : u8 { Solid, Rounded, Border, Shadow, Image, Glyph };
enum class CanvasBlend : u8 { PremultipliedAlpha, Opaque };
enum class CanvasSampler : u8 { LinearClamp, NearestClamp };

struct CanvasBatch final {
    u32 first_index{};
    u32 index_count{};
    CanvasPrimitive primitive{CanvasPrimitive::Solid};
    CanvasImageSource image;
    CanvasRect scissor;
    CanvasBlend blend{CanvasBlend::PremultipliedAlpha};
    CanvasSampler sampler{CanvasSampler::LinearClamp};
};

struct CanvasFrame final {
    SurfaceHandle surface;
    u32 width{};
    u32 height{};
    f32 content_scale{1.0f};
    std::vector<CanvasVertex> vertices;
    std::vector<u32> indices;
    std::vector<CanvasBatch> batches;
    [[nodiscard]] bool Valid() const noexcept;
};

} // namespace woki::gfx
