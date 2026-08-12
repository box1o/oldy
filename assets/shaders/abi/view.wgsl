struct ViewData {
    view_projection: mat4x4f,
    inverse_view_projection: mat4x4f,
    camera_position: vec3f,
    near_plane: f32,
    viewport_size: vec2f,
    far_plane: f32,
    _padding: f32,
};
@group(1) @binding(0) var<uniform> view: ViewData;
