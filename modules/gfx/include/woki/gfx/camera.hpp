#pragma once

#include <optional>
#include <variant>

#include <woki/core.hpp>
#include <woki/math/math.hpp>

namespace woki::gfx {

struct PixelRect {
    u32 x{};
    u32 y{};
    u32 width{};
    u32 height{};

    [[nodiscard]] bool Contains(u32 px, u32 py) const noexcept;
    [[nodiscard]] Result<f32> Aspect() const;
};

struct CameraViewport {
    f32 x{};
    f32 y{};
    f32 width{1.0f};
    f32 height{1.0f};

    [[nodiscard]] Result<PixelRect> Resolve(u32 framebuffer_width, u32 framebuffer_height) const;
    [[nodiscard]] Result<bool> HitTest(f32 logical_x, f32 logical_y, f32 content_scale_x, f32 content_scale_y, u32 framebuffer_width, u32 framebuffer_height) const;
};

struct CameraPose {
    math::vec3f position{};
    math::quat<f32> orientation{0.0f, 0.0f, 0.0f, 1.0f};

    [[nodiscard]] math::vec3f Forward() const noexcept;
    [[nodiscard]] math::vec3f Right() const noexcept;
    [[nodiscard]] math::vec3f Up() const noexcept;
    [[nodiscard]] Result<void> LookAt(const math::vec3f& target, const math::vec3f& world_up = {0.0f, 1.0f, 0.0f});
    [[nodiscard]] Result<math::mat4f> ViewMatrix() const;
};

struct PerspectiveCamera {
    f32 vertical_fov{math::radians(60.0f)};
    f32 near_plane{0.1f};
    f32 far_plane{1000.0f};
    f32 sensor_height_mm{24.0f};
    math::vec2f lens_shift{};
    f32 focus_distance{10.0f};
    f32 aperture_f_number{2.8f};
    f32 exposure_ev100{10.0f};

    [[nodiscard]] Result<f32> FocalLengthMm() const;
    [[nodiscard]] Result<void> SetFocalLengthMm(f32 focal_length_mm);
    [[nodiscard]] Result<math::mat4f> Projection(f32 aspect) const;
};

struct OrthographicCamera {
    f32 height{10.0f};
    f32 near_plane{0.1f};
    f32 far_plane{1000.0f};
    math::vec2f lens_shift{};
    f32 focus_distance{10.0f};
    f32 aperture_f_number{2.8f};
    f32 exposure_ev100{10.0f};

    [[nodiscard]] Result<math::mat4f> Projection(f32 aspect) const;
};

using CameraProjection = std::variant<PerspectiveCamera, OrthographicCamera>;

[[nodiscard]] Result<math::mat4f> PerspectiveWebGpuRH(f32 vertical_fov, f32 aspect, f32 near_plane, f32 far_plane, math::vec2f lens_shift = {});
[[nodiscard]] Result<math::mat4f> OrthographicWebGpuRH(f32 height, f32 aspect, f32 near_plane, f32 far_plane, math::vec2f lens_shift = {});

struct ScreenRay {
    math::vec3f origin;
    math::vec3f direction;
};

struct CameraView {
    CameraPose pose;
    CameraProjection projection;
    CameraViewport viewport;
    PixelRect pixel_rect;
    math::mat4f view;
    math::mat4f projection_matrix;
    math::mat4f view_projection;
    math::mat4f inverse_view;
    math::mat4f inverse_projection;
    math::mat4f inverse_view_projection;
    math::mat4f previous_view_projection;
    math::vec2f jitter_ndc{};

    [[nodiscard]] static Result<CameraView> Build(const CameraPose& pose,
        const CameraProjection& projection,
        const CameraViewport& viewport,
        u32 framebuffer_width,
        u32 framebuffer_height,
        math::vec2f jitter_ndc = {},
        std::optional<math::mat4f> previous_view_projection = std::nullopt);
    [[nodiscard]] Result<math::vec3f> Project(const math::vec3f& world) const;
    [[nodiscard]] Result<math::vec3f> Unproject(const math::vec3f& framebuffer_point) const;
    [[nodiscard]] Result<ScreenRay> Ray(f32 framebuffer_x, f32 framebuffer_y) const;
};

struct OrbitController {
    math::vec3f target{};
    f32 yaw{};
    f32 pitch{0.35f};
    f32 distance{6.0f};
    f32 rotate_sensitivity{0.008f};
    f32 pan_sensitivity{0.002f};
    f32 zoom_sensitivity{0.15f};
    f32 min_pitch{-1.5533f};
    f32 max_pitch{1.5533f};
    f32 min_distance{0.05f};
    f32 max_distance{10000.0f};

    void Rotate(f32 delta_x, f32 delta_y) noexcept;
    void Pan(f32 delta_x, f32 delta_y, CameraPose& pose) noexcept;
    void Dolly(f32 amount) noexcept;
    [[nodiscard]] Result<void> Focus(const math::vec3f& center, f32 radius, f32 vertical_fov = math::radians(60.0f));
    [[nodiscard]] Result<void> Update(CameraPose& pose) noexcept;
};

struct FlyInput {
    f32 right{};
    f32 up{};
    f32 forward{};
    f32 look_x{};
    f32 look_y{};
    bool boost{};
};

struct FlyController {
    f32 yaw{};
    f32 pitch{};
    math::vec3f velocity{};
    f32 look_sensitivity{0.0025f};
    f32 acceleration{25.0f};
    f32 damping{8.0f};
    f32 max_speed{8.0f};
    f32 boost_multiplier{4.0f};
    f32 min_pitch{-1.5533f};
    f32 max_pitch{1.5533f};

    [[nodiscard]] Result<void> Update(CameraPose& pose, const FlyInput& input, f32 delta_seconds) noexcept;
    void Stop() noexcept;
};

} // namespace woki::gfx
