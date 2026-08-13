fn select_shadow_cascade(view_depth: f32, splits: vec4f, cascade_count: u32) -> u32 {
    var cascade = 0u;
    if (cascade_count > 1u && view_depth > splits.x) {
        cascade = 1u;
    }
    if (cascade_count > 2u && view_depth > splits.y) {
        cascade = 2u;
    }
    if (cascade_count > 3u && view_depth > splits.z) {
        cascade = 3u;
    }
    return cascade;
}

fn sample_shadow_pcf(
    shadow_map: texture_depth_2d_array,
    shadow_sampler: sampler_comparison,
    uv: vec2f,
    layer: i32,
    depth: f32,
    texel_size: vec2f
) -> f32 {
    var visibility = 0.0;
    for (var y = -1; y <= 1; y += 1) {
        for (var x = -1; x <= 1; x += 1) {
            visibility += textureSampleCompare(
                shadow_map,
                shadow_sampler,
                uv + vec2f(f32(x), f32(y)) * texel_size,
                layer,
                depth
            );
        }
    }
    return visibility / 9.0;
}

fn shadow_receiver_bias(normal: vec3f, light_direction: vec3f, texel_size: vec2f) -> f32 {
    let slope = 1.0 - saturate(dot(normal, light_direction));
    return max(texel_size.x, texel_size.y) * (0.75 + 2.0 * slope);
}
