@vertex
fn copy_vs(@builtin(vertex_index) vertex_index: u32) -> FullscreenVaryings {
    return fullscreen_vertex(vertex_index);
}

@fragment
fn copy_fs(input: FullscreenVaryings) -> @location(0) vec4f {
    return textureSample(source_texture, source_sampler, input.uv);
}
