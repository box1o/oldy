fn transform_point(matrix: mat4x4f, point: vec3f) -> vec3f {
    return (matrix * vec4f(point, 1.0)).xyz;
}

fn transform_vector(matrix: mat4x4f, vector: vec3f) -> vec3f {
    return (matrix * vec4f(vector, 0.0)).xyz;
}

fn clip_velocity(current_clip: vec4f, previous_clip: vec4f) -> vec2f {
    let current_ndc = current_clip.xy / max(abs(current_clip.w), 0.000001) * sign(current_clip.w);
    let previous_ndc = previous_clip.xy / max(abs(previous_clip.w), 0.000001) * sign(previous_clip.w);
    return (current_ndc - previous_ndc) * vec2f(0.5, -0.5);
}

fn identity_matrix() -> mat4x4f {
    return mat4x4f(
        vec4f(1.0, 0.0, 0.0, 0.0),
        vec4f(0.0, 1.0, 0.0, 0.0),
        vec4f(0.0, 0.0, 1.0, 0.0),
        vec4f(0.0, 0.0, 0.0, 1.0)
    );
}

fn ndc_to_uv(ndc: vec2f) -> vec2f {
    return vec2f(ndc.x * 0.5 + 0.5, 0.5 - ndc.y * 0.5);
}

fn uv_to_ndc(uv: vec2f) -> vec2f {
    return vec2f(uv.x * 2.0 - 1.0, 1.0 - uv.y * 2.0);
}

fn linearize_depth(depth: f32, near_plane: f32, far_plane: f32) -> f32 {
    return near_plane * far_plane
        / max(far_plane - depth * (far_plane - near_plane), 0.000001);
}

fn reconstruct_world_position(
    uv: vec2f,
    depth: f32,
    inverse_view_projection: mat4x4f
) -> vec3f {
    let world = inverse_view_projection * vec4f(uv_to_ndc(uv), depth, 1.0);
    return world.xyz / max(abs(world.w), 0.000001) * sign(world.w);
}
