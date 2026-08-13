@vertex
fn taa_vs(@builtin(vertex_index) vertex_index: u32) -> FullscreenVaryings {
    return fullscreen_vertex(vertex_index);
}

fn taa_current(uv: vec2f) -> vec3f {
    return textureSample(source_texture, source_sampler, uv).rgb;
}

@fragment
fn taa_fs(input: FullscreenVaryings) -> @location(0) vec4f {
    let dimensions = vec2i(textureDimensions(depth_texture));
    let pixel = clamp(vec2i(input.uv * vec2f(dimensions)), vec2i(0), dimensions - 1);
    let depth = textureLoad(depth_texture, pixel, 0);
    let velocity = textureSample(velocity_texture, source_sampler, input.uv).xy;
    let history_uv = input.uv - velocity + post.parameters.zw;
    var neighborhood_min = vec3f(1e20);
    var neighborhood_max = vec3f(-1e20);
    for (var y = -1; y <= 1; y += 1) {
        for (var x = -1; x <= 1; x += 1) {
            let sample_uv = input.uv + vec2f(f32(x), f32(y)) * post.texel_size;
            let color = taa_current(sample_uv);
            neighborhood_min = min(neighborhood_min, color);
            neighborhood_max = max(neighborhood_max, color);
        }
    }
    let current = taa_current(input.uv);
    let inside = all(history_uv >= vec2f(0.0)) && all(history_uv <= vec2f(1.0)) && depth < 1.0 && post.parameters.y > 0.5;
    let history = clamp(textureSample(bloom_texture, source_sampler, history_uv).rgb, neighborhood_min, neighborhood_max);
    return vec4f(mix(current, history, select(0.0, clamp(post.parameters.x, 0.0, 0.99), inside)), 1.0);
}
