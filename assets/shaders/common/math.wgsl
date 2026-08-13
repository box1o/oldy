const WOKI_PI: f32 = 3.141592653589793;
const WOKI_TAU: f32 = 6.283185307179586;
const WOKI_EPSILON: f32 = 0.000001;

fn saturate(value: f32) -> f32 {
    return clamp(value, 0.0, 1.0);
}

fn saturate3(value: vec3f) -> vec3f {
    return clamp(value, vec3f(0.0), vec3f(1.0));
}

fn safe_rcp(value: f32) -> f32 {
    return select(1.0 / value, 0.0, abs(value) < WOKI_EPSILON);
}

fn safe_sqrt(value: f32) -> f32 {
    return sqrt(max(value, 0.0));
}

fn safe_normalize2(value: vec2f) -> vec2f {
    let length_squared = dot(value, value);
    return select(
        value * inverseSqrt(max(length_squared, WOKI_EPSILON)),
        vec2f(0.0),
        length_squared < WOKI_EPSILON
    );
}

fn safe_normalize3(value: vec3f) -> vec3f {
    let length_squared = dot(value, value);
    return select(
        value * inverseSqrt(max(length_squared, WOKI_EPSILON)),
        vec3f(0.0),
        length_squared < WOKI_EPSILON
    );
}

fn pow5(value: f32) -> f32 {
    let squared = value * value;
    return squared * squared * value;
}

fn remap(value: f32, source_min: f32, source_max: f32, target_min: f32, target_max: f32) -> f32 {
    return mix(target_min, target_max, (value - source_min) * safe_rcp(source_max - source_min));
}
