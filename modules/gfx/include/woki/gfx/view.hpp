#pragma once

#include <span>
#include <optional>
#include <vector>

#include <woki/math.hpp>

#include "camera.hpp"
#include "handles.hpp"

namespace woki::gfx {

struct RenderViewTag;
struct ViewHistoryTag;
using ViewId = Handle<RenderViewTag>;
using ViewHistoryId = Handle<ViewHistoryTag>;
struct EnvironmentTag;
using EnvironmentHandle = Handle<EnvironmentTag>;

struct TemporalPolicy final {
    bool taa{true};
    bool jitter{true};
    f32 feedback{0.9F};
    f32 render_scale{1.0F};
    u32 maximum_age{120};
};

enum class ViewFlags : u32 {
    None = 0,
    CameraCut = 1U << 0U,
    Resized = 1U << 1U,
    PipelineChanged = 1U << 2U,
    FormatChanged = 1U << 3U,
    DisableOcclusion = 1U << 4U,
};

[[nodiscard]] constexpr ViewFlags operator|(const ViewFlags left, const ViewFlags right) noexcept {
    return static_cast<ViewFlags>(static_cast<u32>(left) | static_cast<u32>(right));
}

[[nodiscard]] constexpr bool HasFlag(const ViewFlags value, const ViewFlags flag) noexcept {
    return (static_cast<u32>(value) & static_cast<u32>(flag)) != 0;
}

struct CameraState final {
    math::mat4f view{math::mat4f::identity()};
    math::mat4f projection{math::mat4f::identity()};
    math::mat4f view_projection{math::mat4f::identity()};
    math::mat4f previous_view_projection{math::mat4f::identity()};
    math::mat4f inverse_view_projection{math::mat4f::identity()};
    math::vec3f position{};
    f32 near_plane{0.1F};
    f32 far_plane{1000.0F};

    [[nodiscard]] static CameraState FromCameraView(const CameraView& view) noexcept;
};

struct RenderView final {
    ViewId id;
    SceneHandle scene;
    ViewHistoryId history;
    CameraState camera;
    PixelRect viewport;
    u32 output_width{};
    u32 output_height{};
    u32 render_width{};
    u32 render_height{};
    u64 visibility_mask{~u64{0}};
    u64 layer_mask{~u64{0}};
    math::vec2f jitter{};
    f32 exposure{1.0F};
    std::optional<f32> cpu_log_average_luminance;
    EnvironmentHandle environment;
    bool sky_visible{true};
    bool environment_lighting{true};
    TemporalPolicy temporal;
    PipelineHandle pipeline;
    ViewFlags flags{ViewFlags::None};

    [[nodiscard]] bool HistoryInvalid() const noexcept;
};

struct RenderViewFamily final {
    u64 family_id{};
    std::vector<RenderView> views;
    bool shared_preparation{true};
};

} // namespace woki::gfx
