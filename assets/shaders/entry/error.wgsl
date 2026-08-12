struct ErrorVertexOutput { @builtin(position) position: vec4f };
@vertex fn error_vs(@builtin(vertex_index) vertex_index: u32) -> ErrorVertexOutput {
    let positions = array<vec2f, 3>(vec2f(-1.0, -1.0), vec2f(3.0, -1.0), vec2f(-1.0, 3.0));
    return ErrorVertexOutput(vec4f(positions[vertex_index], 0.0, 1.0));
}
@fragment fn error_fs() -> @location(0) vec4f { return vec4f(1.0, 0.0, 1.0, 1.0); }
