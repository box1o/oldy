#include <woki/gfx/view.hpp>

namespace woki::gfx {

CameraState CameraState::FromCameraView(const CameraView& view) noexcept {
    return {
        .view = view.view,
        .projection = view.projection_matrix,
        .view_projection = view.view_projection,
        .previous_view_projection = view.previous_view_projection,
        .inverse_view_projection = view.inverse_view_projection,
        .position = view.pose.position,
        .near_plane = std::visit([](const auto& projection) { return projection.near_plane; }, view.projection),
        .far_plane = std::visit([](const auto& projection) { return projection.far_plane; }, view.projection),
    };
}

bool RenderView::HistoryInvalid() const noexcept {
    constexpr auto invalid = ViewFlags::CameraCut | ViewFlags::Resized | ViewFlags::PipelineChanged | ViewFlags::FormatChanged;
    return (static_cast<u32>(flags) & static_cast<u32>(invalid)) != 0;
}

} // namespace woki::gfx
