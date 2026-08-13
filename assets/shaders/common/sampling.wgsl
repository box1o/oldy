fn sample_texture_2d(texture_value: texture_2d<f32>, sampler_value: sampler, uv: vec2f) -> vec4f {
    return textureSample(texture_value, sampler_value, uv);
}

fn sample_texture_2d_level(
    texture_value: texture_2d<f32>,
    sampler_value: sampler,
    uv: vec2f,
    level: f32
) -> vec4f {
    return textureSampleLevel(texture_value, sampler_value, uv, level);
}

fn sample_equirectangular(
    texture_value: texture_2d<f32>,
    sampler_value: sampler,
    direction: vec3f
) -> vec3f {
    let normalized = normalize(direction);
    let uv = vec2f(
        atan2(normalized.z, normalized.x) / WOKI_TAU + 0.5,
        acos(clamp(normalized.y, -1.0, 1.0)) / WOKI_PI
    );
    return textureSample(texture_value, sampler_value, uv).rgb;
}
