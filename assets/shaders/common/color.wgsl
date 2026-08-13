fn linear_to_srgb_channel(value: f32) -> f32 {
    let x = max(value, 0.0);
    return select(1.055 * pow(x, 1.0 / 2.4) - 0.055, 12.92 * x, x <= 0.0031308);
}

fn linear_to_srgb(value: vec3f) -> vec3f {
    return vec3f(
        linear_to_srgb_channel(value.r),
        linear_to_srgb_channel(value.g),
        linear_to_srgb_channel(value.b)
    );
}

fn srgb_to_linear_channel(value: f32) -> f32 {
    let x = max(value, 0.0);
    return select(pow((x + 0.055) / 1.055, 2.4), x / 12.92, x <= 0.04045);
}

fn srgb_to_linear(value: vec3f) -> vec3f {
    return vec3f(
        srgb_to_linear_channel(value.r),
        srgb_to_linear_channel(value.g),
        srgb_to_linear_channel(value.b)
    );
}

fn luminance(value: vec3f) -> f32 {
    return dot(value, vec3f(0.2126, 0.7152, 0.0722));
}

fn apply_exposure(value: vec3f, exposure_ev: f32) -> vec3f {
    return value * exp2(exposure_ev);
}

fn tone_map_aces(value: vec3f) -> vec3f {
    let a = 2.51;
    let b = 0.03;
    let c = 2.43;
    let d = 0.59;
    let e = 0.14;
    return clamp(
        (value * (a * value + vec3f(b)))
            / (value * (c * value + vec3f(d)) + vec3f(e)),
        vec3f(0.0),
        vec3f(1.0)
    );
}

fn tone_map_reinhard(value: vec3f) -> vec3f {
    return value / (vec3f(1.0) + value);
}
