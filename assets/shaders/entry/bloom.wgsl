@vertex
fn bloom_vs(@builtin(vertex_index) vertex_index: u32) -> FullscreenVaryings {
    return fullscreen_vertex(vertex_index);
}

@fragment
fn bloom_threshold_fs(input: FullscreenVaryings) -> @location(0) vec4f {
    return vec4f(
        bloom_threshold(
            textureSample(source_texture, source_sampler, input.uv).rgb,
            post.parameters.x,
            post.parameters.y
        ),
        1.0
    );
}

@fragment
fn bloom_downsample_fs(input: FullscreenVaryings) -> @location(0) vec4f {
    let offset = post.texel_size * 0.5;
    let color = textureSample(source_texture, source_sampler, input.uv + vec2f(-offset.x, -offset.y)).rgb
        + textureSample(source_texture, source_sampler, input.uv + vec2f(offset.x, -offset.y)).rgb
        + textureSample(source_texture, source_sampler, input.uv + vec2f(-offset.x, offset.y)).rgb
        + textureSample(source_texture, source_sampler, input.uv + vec2f(offset.x, offset.y)).rgb;
    return vec4f(color * 0.25, 1.0);
}

@fragment
fn bloom_blur_fs(input: FullscreenVaryings) -> @location(0) vec4f {
    let direction = post.texel_size * post.parameters.zw;
    var color = textureSample(source_texture, source_sampler, input.uv).rgb * 0.227027;
    color += textureSample(
        source_texture,
        source_sampler,
        input.uv + direction * 1.384615
    ).rgb * 0.316216;
    color += textureSample(
        source_texture,
        source_sampler,
        input.uv - direction * 1.384615
    ).rgb * 0.316216;
    color += textureSample(
        source_texture,
        source_sampler,
        input.uv + direction * 3.230769
    ).rgb * 0.070270;
    color += textureSample(
        source_texture,
        source_sampler,
        input.uv - direction * 3.230769
    ).rgb * 0.070270;
    return vec4f(color, 1.0);
}

@fragment
fn bloom_composite_fs(input: FullscreenVaryings) -> @location(0) vec4f {
    let scene_color = textureSample(source_texture, source_sampler, input.uv).rgb;
    let bloom_color = textureSample(bloom_texture, source_sampler, input.uv).rgb;
    return vec4f(bloom_composite(scene_color, bloom_color, post.parameters.x), 1.0);
}
