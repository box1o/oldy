fn debug_normal(normal: vec3f) -> vec3f { return normal * 0.5 + vec3f(0.5); }
fn debug_depth(depth: f32) -> vec3f { return vec3f(saturate(depth)); }
fn is_invalid(value: f32) -> bool { return (bitcast<u32>(value) & 0x7fffffffu) >= 0x7f800000u; }
fn debug_invalid(value: vec3f, valid_color: vec3f) -> vec3f {
    return select(valid_color, vec3f(1.0, 0.0, 1.0), any(vec3<bool>(is_invalid(value.x), is_invalid(value.y), is_invalid(value.z))));
}
