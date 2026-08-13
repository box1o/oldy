@vertex
fn fxaa_vs(@builtin(vertex_index) vertex_index: u32) -> FullscreenVaryings {
    return fullscreen_vertex(vertex_index);
}

fn fxaa_luma(color: vec3f) -> f32 {
    return dot(color, vec3f(0.299, 0.587, 0.114));
}

@fragment
fn fxaa_fs(input: FullscreenVaryings) -> @location(0) vec4f {
    let center = textureSampleLevel(source_texture, source_sampler, input.uv, 0.0).rgb;
    let north = textureSampleLevel(source_texture, source_sampler, input.uv + vec2f(0.0, -post.texel_size.y), 0.0).rgb;
    let south = textureSampleLevel(source_texture, source_sampler, input.uv + vec2f(0.0, post.texel_size.y), 0.0).rgb;
    let west = textureSampleLevel(source_texture, source_sampler, input.uv + vec2f(-post.texel_size.x, 0.0), 0.0).rgb;
    let east = textureSampleLevel(source_texture, source_sampler, input.uv + vec2f(post.texel_size.x, 0.0), 0.0).rgb;
    let minimum = min(fxaa_luma(center), min(min(fxaa_luma(north), fxaa_luma(south)), min(fxaa_luma(west), fxaa_luma(east))));
    let maximum = max(fxaa_luma(center), max(max(fxaa_luma(north), fxaa_luma(south)), max(fxaa_luma(west), fxaa_luma(east))));
    if (maximum - minimum < max(0.0312, maximum * 0.125)) {
        return vec4f(center, 1.0);
    }
    let horizontal = abs(fxaa_luma(north) + fxaa_luma(south) - 2.0 * fxaa_luma(center));
    let vertical = abs(fxaa_luma(west) + fxaa_luma(east) - 2.0 * fxaa_luma(center));
    let direction = select(vec2f(post.texel_size.x, 0.0), vec2f(0.0, post.texel_size.y), horizontal >= vertical);
    return vec4f((textureSampleLevel(source_texture, source_sampler, input.uv - direction * 0.5, 0.0).rgb
        + textureSampleLevel(source_texture, source_sampler, input.uv + direction * 0.5, 0.0).rgb) * 0.5, 1.0);
}
