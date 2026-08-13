fn build_tbn(normal: vec3f, tangent: vec4f) -> mat3x3f {
    let n = safe_normalize3(normal);
    let t = safe_normalize3(tangent.xyz - n * dot(n, tangent.xyz));
    return mat3x3f(t, cross(n, t) * tangent.w, n);
}

fn unpack_tangent_normal(sample_value: vec3f, normal_scale: f32) -> vec3f {
    let xy = (sample_value.xy * 2.0 - vec2f(1.0)) * normal_scale;
    return safe_normalize3(vec3f(xy, safe_sqrt(1.0 - dot(xy, xy))));
}

fn apply_normal_map(
    world_normal: vec3f,
    world_tangent: vec4f,
    sample_value: vec3f,
    normal_scale: f32
) -> vec3f {
    return safe_normalize3(
        build_tbn(world_normal, world_tangent)
            * unpack_tangent_normal(sample_value, normal_scale)
    );
}
