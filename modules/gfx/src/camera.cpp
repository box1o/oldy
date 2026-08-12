#include <woki/gfx/camera.hpp>

#include <algorithm>
#include <cmath>
#include <limits>

namespace woki::gfx {
namespace {

constexpr f32 kEpsilon = 1.0e-6f;

bool Finite(f32 value) {
    return std::isfinite(value);
}

bool Finite(const math::vec2f& value) {
    return Finite(value.x) && Finite(value.y);
}

bool Finite(const math::vec3f& value) {
    return Finite(value.x) && Finite(value.y) && Finite(value.z);
}

bool Finite(const math::quat<f32>& value) {
    return Finite(value.x) && Finite(value.y) && Finite(value.z) && Finite(value.w);
}

Result<void> ValidatePlanes(f32 near_plane, f32 far_plane) {
    if (!Finite(near_plane) || !Finite(far_plane) || near_plane <= 0.0f || far_plane <= near_plane)
        return Err(ErrorCode::InvalidArgument, "camera planes must be finite and satisfy 0 < near < far");
    return Ok();
}

Result<void> ValidateLensMetadata(f32 focus_distance, f32 aperture_f_number, f32 exposure_ev100) {
    if (!Finite(focus_distance) || !Finite(aperture_f_number) || !Finite(exposure_ev100) || focus_distance <= 0.0f || aperture_f_number <= 0.0f)
        return Err(ErrorCode::InvalidArgument, "camera lens metadata must be finite with positive focus distance and aperture");
    return Ok();
}

Result<math::mat4f> ReliableInverse(const math::mat4f& matrix) {
    const math::mat4f inverse = matrix.inverse();
    for (u32 row = 0; row < 4; ++row)
        for (u32 column = 0; column < 4; ++column)
            if (!Finite(inverse(row, column)))
                return Err(ErrorCode::InvalidArgument, "camera matrix inverse is not finite");

    const auto reliable_product = [](const math::mat4f& lhs, const math::mat4f& rhs) {
        for (u32 row = 0; row < 4; ++row) {
            for (u32 column = 0; column < 4; ++column) {
                f32 product = 0.0f;
                f32 scale = 0.0f;
                for (u32 index = 0; index < 4; ++index) {
                    product += lhs(row, index) * rhs(index, column);
                    scale += std::abs(lhs(row, index)) * std::abs(rhs(index, column));
                }
                const f32 expected = row == column ? 1.0f : 0.0f;
                const f32 tolerance = 64.0f * std::numeric_limits<f32>::epsilon() * std::max(1.0f, scale);
                if (!Finite(product) || std::abs(product - expected) > tolerance)
                    return false;
            }
        }
        return true;
    };
    if (!reliable_product(matrix, inverse) || !reliable_product(inverse, matrix))
        return Err(ErrorCode::InvalidArgument, "camera matrix inverse is unreliable");
    return inverse;
}

Result<math::vec3f> TransformPoint(const math::mat4f& matrix, const math::vec3f& point) {
    const math::vec4f transformed = matrix * math::vec4f(point, 1.0f);
    if (!Finite(transformed.w) || std::abs(transformed.w) <= kEpsilon)
        return Err(ErrorCode::InvalidArgument, "camera homogeneous point has invalid w");
    const math::vec3f result{transformed.x / transformed.w, transformed.y / transformed.w, transformed.z / transformed.w};
    if (!Finite(result))
        return Err(ErrorCode::InvalidArgument, "camera transformed point is not finite");
    return result;
}

} // namespace

bool PixelRect::Contains(u32 px, u32 py) const noexcept {
    return px >= x && py >= y && static_cast<u64>(px) < static_cast<u64>(x) + width && static_cast<u64>(py) < static_cast<u64>(y) + height;
}

Result<f32> PixelRect::Aspect() const {
    if (width == 0 || height == 0)
        return Err(ErrorCode::InvalidArgument, "camera viewport has zero area");
    return static_cast<f32>(width) / static_cast<f32>(height);
}

Result<PixelRect> CameraViewport::Resolve(u32 framebuffer_width, u32 framebuffer_height) const {
    if (!Finite(x) || !Finite(y) || !Finite(width) || !Finite(height) || x < 0.0f || y < 0.0f || width < 0.0f || height < 0.0f || x + width > 1.0f + kEpsilon || y + height > 1.0f + kEpsilon)
        return Err(ErrorCode::InvalidArgument, "camera viewport must be a finite normalized rectangle");
    const auto edge = [](f32 normalized, u32 extent) { return static_cast<u32>(std::lround(std::clamp(normalized, 0.0f, 1.0f) * static_cast<f32>(extent))); };
    const u32 left = edge(x, framebuffer_width);
    const u32 top = edge(y, framebuffer_height);
    const u32 right = edge(x + width, framebuffer_width);
    const u32 bottom = edge(y + height, framebuffer_height);
    return PixelRect{left, top, right - left, bottom - top};
}

Result<bool> CameraViewport::HitTest(f32 logical_x, f32 logical_y, f32 content_scale_x, f32 content_scale_y, u32 framebuffer_width, u32 framebuffer_height) const {
    if (!Finite(logical_x) || !Finite(logical_y) || !Finite(content_scale_x) || !Finite(content_scale_y) || content_scale_x <= 0.0f || content_scale_y <= 0.0f)
        return Err(ErrorCode::InvalidArgument, "camera hit test coordinates and content scale must be finite");
    const auto rect = Resolve(framebuffer_width, framebuffer_height);
    if (!rect)
        return Err(rect.error());
    const f32 px = logical_x * content_scale_x;
    const f32 py = logical_y * content_scale_y;
    return px >= static_cast<f32>(rect->x) && py >= static_cast<f32>(rect->y) && px < static_cast<f32>(rect->x + rect->width) && py < static_cast<f32>(rect->y + rect->height);
}

math::vec3f CameraPose::Forward() const noexcept {
    return orientation.rotate({0.0f, 0.0f, -1.0f});
}

math::vec3f CameraPose::Right() const noexcept {
    return orientation.rotate({1.0f, 0.0f, 0.0f});
}

math::vec3f CameraPose::Up() const noexcept {
    return orientation.rotate({0.0f, 1.0f, 0.0f});
}

Result<void> CameraPose::LookAt(const math::vec3f& target, const math::vec3f& world_up) {
    if (!Finite(position) || !Finite(target) || !Finite(world_up))
        return Err(ErrorCode::InvalidArgument, "camera look-at inputs must be finite");
    const math::vec3f forward = target - position;
    if (forward.length_squared() <= kEpsilon * kEpsilon || world_up.length_squared() <= kEpsilon * kEpsilon)
        return Err(ErrorCode::InvalidArgument, "camera look direction and up vector must be non-degenerate");
    const math::vec3f f = forward.normalized();
    const math::vec3f unnormalized_right = f.cross(world_up.normalized());
    if (unnormalized_right.length_squared() <= kEpsilon * kEpsilon)
        return Err(ErrorCode::InvalidArgument, "camera look direction is parallel to up");
    const math::vec3f right = unnormalized_right.normalized();
    const math::vec3f up = right.cross(f);
    const math::mat4f rotation(math::layout::rowm, right.x, up.x, -f.x, 0.0f, right.y, up.y, -f.y, 0.0f, right.z, up.z, -f.z, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f);
    orientation = math::quat<f32>::fromMat4(rotation);
    return Ok();
}

Result<math::mat4f> CameraPose::ViewMatrix() const {
    if (!Finite(position) || !Finite(orientation) || orientation.length_squared() <= kEpsilon * kEpsilon)
        return Err(ErrorCode::InvalidArgument, "camera pose must be finite with a non-zero orientation");
    const auto normalized = orientation.normalized();
    return normalized.conjugate().toMat4() * math::translate(-position);
}

Result<math::mat4f> PerspectiveWebGpuRH(f32 vertical_fov, f32 aspect, f32 near_plane, f32 far_plane, math::vec2f lens_shift) {
    if (!Finite(vertical_fov) || !Finite(aspect) || !Finite(lens_shift) || vertical_fov <= 0.0f || vertical_fov >= math::pi<f32> || aspect <= 0.0f)
        return Err(ErrorCode::InvalidArgument, "perspective FOV and aspect must be finite and positive");
    TRY_VOID(ValidatePlanes(near_plane, far_plane));
    const f32 tan_half = std::tan(vertical_fov * 0.5f);
    const f32 depth = near_plane - far_plane;
    return math::mat4f(math::layout::rowm, 1.0f / (aspect * tan_half), 0.0f, lens_shift.x * 2.0f, 0.0f, 0.0f, 1.0f / tan_half, lens_shift.y * 2.0f, 0.0f, 0.0f, 0.0f, far_plane / depth, near_plane * far_plane / depth,
        0.0f, 0.0f, -1.0f, 0.0f);
}

Result<math::mat4f> OrthographicWebGpuRH(f32 height, f32 aspect, f32 near_plane, f32 far_plane, math::vec2f lens_shift) {
    if (!Finite(height) || !Finite(aspect) || !Finite(lens_shift) || height <= 0.0f || aspect <= 0.0f)
        return Err(ErrorCode::InvalidArgument, "orthographic height and aspect must be finite and positive");
    TRY_VOID(ValidatePlanes(near_plane, far_plane));
    return math::mat4f(math::layout::rowm, 2.0f / (height * aspect), 0.0f, 0.0f, -lens_shift.x * 2.0f, 0.0f, 2.0f / height, 0.0f, -lens_shift.y * 2.0f, 0.0f, 0.0f, 1.0f / (near_plane - far_plane),
        near_plane / (near_plane - far_plane), 0.0f, 0.0f, 0.0f, 1.0f);
}

Result<f32> PerspectiveCamera::FocalLengthMm() const {
    if (!Finite(sensor_height_mm) || sensor_height_mm <= 0.0f || !Finite(vertical_fov) || vertical_fov <= 0.0f || vertical_fov >= math::pi<f32>)
        return Err(ErrorCode::InvalidArgument, "physical lens requires a positive sensor height and FOV in (0, pi)");
    return sensor_height_mm / (2.0f * std::tan(vertical_fov * 0.5f));
}

Result<void> PerspectiveCamera::SetFocalLengthMm(f32 focal_length_mm) {
    if (!Finite(focal_length_mm) || !Finite(sensor_height_mm) || focal_length_mm <= 0.0f || sensor_height_mm <= 0.0f)
        return Err(ErrorCode::InvalidArgument, "focal length and sensor height must be finite and positive");
    vertical_fov = 2.0f * std::atan(sensor_height_mm / (2.0f * focal_length_mm));
    return Ok();
}

Result<math::mat4f> PerspectiveCamera::Projection(f32 aspect) const {
    TRY_VOID(ValidateLensMetadata(focus_distance, aperture_f_number, exposure_ev100));
    return PerspectiveWebGpuRH(vertical_fov, aspect, near_plane, far_plane, lens_shift);
}

Result<math::mat4f> OrthographicCamera::Projection(f32 aspect) const {
    TRY_VOID(ValidateLensMetadata(focus_distance, aperture_f_number, exposure_ev100));
    return OrthographicWebGpuRH(height, aspect, near_plane, far_plane, lens_shift);
}

Result<CameraView> CameraView::Build(const CameraPose& pose,
    const CameraProjection& projection,
    const CameraViewport& viewport,
    u32 framebuffer_width,
    u32 framebuffer_height,
    math::vec2f jitter_ndc,
    std::optional<math::mat4f> previous_view_projection) {
    if (!Finite(jitter_ndc))
        return Err(ErrorCode::InvalidArgument, "camera jitter must be finite");
    CameraView result;
    result.pose = pose;
    result.projection = projection;
    result.viewport = viewport;
    result.jitter_ndc = jitter_ndc;
    TRY_ASSIGN(result.pixel_rect, viewport.Resolve(framebuffer_width, framebuffer_height));
    f32 aspect{};
    TRY_ASSIGN(aspect, result.pixel_rect.Aspect());
    TRY_ASSIGN(result.view, pose.ViewMatrix());
    auto projected = std::visit([aspect](const auto& value) { return value.Projection(aspect); }, projection);
    if (!projected)
        return Err(projected.error());
    result.projection_matrix = *projected;
    if (std::holds_alternative<PerspectiveCamera>(projection)) {
        result.projection_matrix(0, 2) -= jitter_ndc.x;
        result.projection_matrix(1, 2) -= jitter_ndc.y;
    } else {
        result.projection_matrix(0, 3) += jitter_ndc.x;
        result.projection_matrix(1, 3) += jitter_ndc.y;
    }
    result.view_projection = result.projection_matrix * result.view;
    TRY_ASSIGN(result.inverse_view, ReliableInverse(result.view));
    TRY_ASSIGN(result.inverse_projection, ReliableInverse(result.projection_matrix));
    TRY_ASSIGN(result.inverse_view_projection, ReliableInverse(result.view_projection));
    result.previous_view_projection = previous_view_projection.value_or(result.view_projection);
    return result;
}

Result<math::vec3f> CameraView::Project(const math::vec3f& world) const {
    if (!Finite(world) || pixel_rect.width == 0 || pixel_rect.height == 0)
        return Err(ErrorCode::InvalidArgument, "project requires a finite point and non-empty viewport");
    auto ndc = TransformPoint(view_projection, world);
    if (!ndc)
        return Err(ndc.error());
    return math::vec3f{static_cast<f32>(pixel_rect.x) + (ndc->x * 0.5f + 0.5f) * static_cast<f32>(pixel_rect.width), static_cast<f32>(pixel_rect.y) + (0.5f - ndc->y * 0.5f) * static_cast<f32>(pixel_rect.height), ndc->z};
}

Result<math::vec3f> CameraView::Unproject(const math::vec3f& framebuffer_point) const {
    if (!Finite(framebuffer_point) || pixel_rect.width == 0 || pixel_rect.height == 0 || framebuffer_point.z < 0.0f || framebuffer_point.z > 1.0f)
        return Err(ErrorCode::InvalidArgument, "unproject requires finite framebuffer coordinates and WebGPU depth in [0, 1]");
    const math::vec3f ndc{(framebuffer_point.x - static_cast<f32>(pixel_rect.x)) * 2.0f / static_cast<f32>(pixel_rect.width) - 1.0f,
        1.0f - (framebuffer_point.y - static_cast<f32>(pixel_rect.y)) * 2.0f / static_cast<f32>(pixel_rect.height), framebuffer_point.z};
    return TransformPoint(inverse_view_projection, ndc);
}

Result<ScreenRay> CameraView::Ray(f32 framebuffer_x, f32 framebuffer_y) const {
    math::vec3f near_point;
    math::vec3f far_point;
    TRY_ASSIGN(near_point, Unproject({framebuffer_x, framebuffer_y, 0.0f}));
    TRY_ASSIGN(far_point, Unproject({framebuffer_x, framebuffer_y, 1.0f}));
    const math::vec3f delta = far_point - near_point;
    if (delta.length_squared() <= kEpsilon * kEpsilon)
        return Err(ErrorCode::InvalidArgument, "screen ray is degenerate");
    const bool perspective = std::holds_alternative<PerspectiveCamera>(projection);
    return ScreenRay{perspective ? pose.position : near_point, delta.normalized()};
}

void OrbitController::Rotate(f32 delta_x, f32 delta_y) noexcept {
    if (!Finite(delta_x) || !Finite(delta_y))
        return;
    yaw -= delta_x * rotate_sensitivity;
    pitch = std::clamp(pitch - delta_y * rotate_sensitivity, min_pitch, max_pitch);
}

void OrbitController::Pan(f32 delta_x, f32 delta_y, CameraPose& pose) noexcept {
    if (!Finite(delta_x) || !Finite(delta_y))
        return;
    const f32 scale = pan_sensitivity * distance;
    target += pose.Right() * (-delta_x * scale) + pose.Up() * (delta_y * scale);
}

void OrbitController::Dolly(f32 amount) noexcept {
    if (Finite(amount))
        distance = std::clamp(distance * std::exp(-amount * zoom_sensitivity), min_distance, max_distance);
}

Result<void> OrbitController::Focus(const math::vec3f& center, f32 radius, f32 vertical_fov) {
    if (!Finite(center) || !Finite(radius) || !Finite(vertical_fov) || radius <= 0.0f || vertical_fov <= 0.0f || vertical_fov >= math::pi<f32>)
        return Err(ErrorCode::InvalidArgument, "orbit focus target, radius, and FOV must be valid");
    target = center;
    distance = std::clamp(radius / std::sin(vertical_fov * 0.5f), min_distance, max_distance);
    return Ok();
}

Result<void> OrbitController::Update(CameraPose& pose) noexcept {
    if (!Finite(target) || !Finite(yaw) || !Finite(pitch) || !Finite(distance) || min_distance <= 0.0f || max_distance < min_distance || min_pitch > max_pitch)
        return Err(ErrorCode::InvalidArgument, "orbit controller state is invalid");
    pitch = std::clamp(pitch, min_pitch, max_pitch);
    distance = std::clamp(distance, min_distance, max_distance);
    const f32 cp = std::cos(pitch);
    pose.position = target + math::vec3f{std::sin(yaw) * cp, std::sin(pitch), std::cos(yaw) * cp} * distance;
    return pose.LookAt(target);
}

Result<void> FlyController::Update(CameraPose& pose, const FlyInput& input, f32 delta_seconds) noexcept {
    if (!Finite(delta_seconds) || delta_seconds < 0.0f || !Finite(input.right) || !Finite(input.up) || !Finite(input.forward) || !Finite(input.look_x) || !Finite(input.look_y))
        return Err(ErrorCode::InvalidArgument, "fly input and delta time must be finite and non-negative");
    yaw -= input.look_x * look_sensitivity;
    pitch = std::clamp(pitch - input.look_y * look_sensitivity, min_pitch, max_pitch);
    const f32 cp = std::cos(pitch);
    const math::vec3f forward{-std::sin(yaw) * cp, std::sin(pitch), -std::cos(yaw) * cp};
    const math::vec3f right{std::cos(yaw), 0.0f, -std::sin(yaw)};
    math::vec3f direction = right * input.right + math::vec3f{0.0f, 1.0f, 0.0f} * input.up + forward * input.forward;
    if (direction.length_squared() > 1.0f)
        direction.normalize();
    const f32 speed = max_speed * (input.boost ? boost_multiplier : 1.0f);
    velocity += direction * (acceleration * speed * delta_seconds);
    const f32 velocity_length = velocity.length();
    if (velocity_length > speed)
        velocity *= speed / velocity_length;
    velocity *= std::exp(-damping * delta_seconds);
    pose.position += velocity * delta_seconds;
    return pose.LookAt(pose.position + forward);
}

void FlyController::Stop() noexcept {
    velocity = {};
}

} // namespace woki::gfx
