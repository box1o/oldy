#pragma once

#include <array>

#include <woki/math.hpp>

#include "draw_packet.hpp"
#include "render_world.hpp"

namespace woki::task {
class Scheduler;
}

namespace woki::gfx {

struct FrustumPlane final {
    math::vec3f normal{};
    f32 distance{};
};

class Frustum final {
public:
    [[nodiscard]] static Frustum FromWebGpuViewProjection(const math::mat4f& matrix) noexcept;
    [[nodiscard]] bool IntersectsSphere(const math::vec3f& center, f32 radius) const noexcept;
    [[nodiscard]] bool IntersectsAabb(const math::vec3f& minimum, const math::vec3f& maximum) const noexcept;

    [[nodiscard]] std::span<const FrustumPlane, 6> Planes() const noexcept {
        return planes_;
    }

private:
    std::array<FrustumPlane, 6> planes_{};
};

struct OcclusionVisibilityHook final {
    using Test = bool (*)(const void* user, RenderObjectId id, const RenderBounds& bounds, const RenderView& view) noexcept;
    const void* user{};
    Test test{};

    // False may remove an object only when the implementation has conservative proof.
    [[nodiscard]] bool PotentiallyVisible(RenderObjectId id, const RenderBounds& bounds, const RenderView& view) const noexcept {
        return test == nullptr || test(user, id, bounds, view);
    }
};

struct VisibleObject final {
    RenderObjectId id;
    u32 dense_index{};
    u32 source_index{};
    u32 lod{};
    RenderPhase phase{RenderPhase::Opaque};
    f32 view_depth{};
};

struct VisibilityResult final {
    std::vector<VisibleObject> visible;
    std::array<std::vector<VisibleObject>, static_cast<size_t>(RenderPhase::Count)> phases;
};

struct VisibilityOptions final {
    f32 lod_error_pixels{1.0F};
    f32 lod_hysteresis{0.15F};
    size_t batch_size{256};
};

class VisibilityService final {
public:
    [[nodiscard]] Result<VisibilityResult> Cull(const RenderWorldSnapshot& world,
        const RenderView& view,
        VisibilityOptions options = {},
        task::Scheduler* scheduler = nullptr,
        const OcclusionVisibilityHook* occlusion = nullptr) const;
};

struct PreparedPhaseDraws final {
    std::array<std::vector<DrawPacket>, static_cast<size_t>(RenderPhase::Count)> phases;
};

// Sources correspond one-for-one with VisibilityResult::visible. Transparent
// objects are built individually so packet sorting cannot break stable depth order.
[[nodiscard]] Result<PreparedPhaseDraws> PrepareDrawPackets(const VisibilityResult& visibility, std::span<const DrawPacketMesh> sources, const DrawPacketBuilder& builder = {});

} // namespace woki::gfx
