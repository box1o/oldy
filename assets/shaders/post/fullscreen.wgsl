struct FullscreenVaryings {
    @builtin(position) position: vec4f,
    @location(0) uv: vec2f
};

fn fullscreen_vertex(vertex_index: u32) -> FullscreenVaryings {
    let uv = vec2f(f32((vertex_index << 1u) & 2u), f32(vertex_index & 2u));
    return FullscreenVaryings(
        vec4f(uv * vec2f(2.0, -2.0) + vec2f(-1.0, 1.0), 0.0, 1.0),
        uv
    );
}
