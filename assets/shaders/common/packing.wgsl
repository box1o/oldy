fn pack_unorm4x8_rgba(value: vec4f) -> u32 { return pack4x8unorm(clamp(value, vec4f(0.0), vec4f(1.0))); }
fn unpack_unorm4x8_rgba(value: u32) -> vec4f { return unpack4x8unorm(value); }
fn encode_octahedral(normal: vec3f) -> vec2f {
    var projected = normal / max(abs(normal.x) + abs(normal.y) + abs(normal.z), 0.000001);
    if (projected.z < 0.0) {
        projected = vec3f((vec2f(1.0) - abs(projected.yx)) * sign(projected.xy), projected.z);
    }
    return projected.xy * 0.5 + vec2f(0.5);
}
fn decode_octahedral(encoded: vec2f) -> vec3f {
    let f = encoded * 2.0 - vec2f(1.0);
    var normal = vec3f(f, 1.0 - abs(f.x) - abs(f.y));
    if (normal.z < 0.0) {
        normal = vec3f((vec2f(1.0) - abs(normal.yx)) * sign(normal.xy), normal.z);
    }
    return normalize(normal);
}
