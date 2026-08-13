#pragma once

#include <array>
#include <optional>

#include <woki/math.hpp>

#include "handles.hpp"

namespace woki::gfx {

struct RenderObjectTag;
struct RenderLightTag;
using RenderObjectId = Handle<RenderObjectTag>;
using RenderLightId = Handle<RenderLightTag>;

enum class RenderObjectFlags : u32 {
    None = 0,
    CastShadow = 1U << 0U,
    ReceiveShadow = 1U << 1U,
    Overlay = 1U << 2U,
    Hidden = 1U << 3U,
};

[[nodiscard]] constexpr RenderObjectFlags operator|(const RenderObjectFlags left, const RenderObjectFlags right) noexcept {
    return static_cast<RenderObjectFlags>(static_cast<u32>(left) | static_cast<u32>(right));
}

enum class RenderPhase : u8 { Depth, Shadow, Opaque, AlphaTest, Transparent, Overlay, Count };

struct RenderBounds final {
    math::vec3f center{};
    f32 radius{};
    math::vec3f minimum{};
    math::vec3f maximum{};
};

struct RenderLodState final {
    static constexpr u32 kMaxLods = 8;
    std::array<f32, kMaxLods> geometric_errors{};
    u32 count{1};
    u32 first_resident{};
    u32 last_resident{};
    u32 previous{};
};

struct RenderObjectData final {
    math::mat4f transform{math::mat4f::identity()};
    math::mat4f previous_transform{math::mat4f::identity()};
    RenderBounds bounds;
    MeshHandle mesh;
    MaterialInstanceHandle material;
    SkinPaletteHandle palette;
    u64 visibility_mask{~u64{0}};
    u64 layers{~u64{0}};
    RenderObjectFlags flags{RenderObjectFlags::CastShadow | RenderObjectFlags::ReceiveShadow};
    MaterialPhase material_phase{MaterialPhase::Opaque};
    RenderLodState lod;
    std::array<u32, 4> feature_payload_ids{};
};

struct RenderObjectPatch final {
    std::optional<math::mat4f> transform;
    std::optional<math::mat4f> previous_transform;
    std::optional<RenderBounds> bounds;
    std::optional<MeshHandle> mesh;
    std::optional<MaterialInstanceHandle> material;
    std::optional<SkinPaletteHandle> palette;
    std::optional<u64> visibility_mask;
    std::optional<u64> layers;
    std::optional<RenderObjectFlags> flags;
    std::optional<MaterialPhase> material_phase;
    std::optional<RenderLodState> lod;
    std::optional<std::array<u32, 4>> feature_payload_ids;
};

enum class LightType : u8 { Directional, Point, Spot };

struct RenderLightData final {
    LightType type{LightType::Point};
    math::vec3f position{};
    f32 range{1.0F};
    math::vec3f direction{0.0F, -1.0F, 0.0F};
    f32 spot_outer_cos{-1.0F};
    math::vec3f color{1.0F};
    f32 intensity{1.0F};
    f32 spot_inner_cos{-1.0F};
    u64 visibility_mask{~u64{0}};
    u32 flags{};
};

} // namespace woki::gfx
