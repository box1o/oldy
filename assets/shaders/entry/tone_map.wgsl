@vertex
fn tone_map_vs(@builtin(vertex_index) vertex_index: u32) -> FullscreenVaryings {
    return fullscreen_vertex(vertex_index);
}

@fragment
fn tone_map_fs(input: FullscreenVaryings) -> @location(0) vec4f {
    let hdr = textureSample(source_texture, source_sampler, input.uv).rgb;
    let linear = tone_map_aces(apply_exposure(hdr, post.parameters.x));
    let lut_color = textureSample(grading_lut, source_sampler, clamp(linear, vec3f(0.0), vec3f(1.0))).rgb;
    let graded = select(linear, lut_color, post.parameters.w > 0.5);
    let encoded = select(graded, linear_to_srgb(graded), post.parameters.y > 0.5);
    return vec4f(encoded, 1.0);
}
