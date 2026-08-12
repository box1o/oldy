@vertex fn tone_map_vs(@builtin(vertex_index) vertex_index: u32) -> FullscreenVaryings { return fullscreen_vertex(vertex_index); }
@fragment fn tone_map_fs(input: FullscreenVaryings) -> @location(0) vec4f {
    let hdr = textureSample(source_texture, source_sampler, input.uv).rgb;
    return vec4f(linear_to_srgb(tone_map_aces(apply_exposure(hdr, post.parameters.x))), 1.0);
}
