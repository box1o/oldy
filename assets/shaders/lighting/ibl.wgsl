fn environment_brdf_approximation(f0: vec3f, roughness: f32, n_dot_v: f32) -> vec3f {
    let r = roughness * vec4f(-1.0, -0.0275, -0.572, 0.022) + vec4f(1.0, 0.0425, 1.04, -0.04);
    let a004 = min(r.x * r.x, exp2(-9.28 * n_dot_v)) * r.x + r.y;
    let ab = vec2f(-1.04, 1.04) * a004 + r.zw;
    return f0 * ab.x + vec3f(ab.y);
}
fn evaluate_ibl(surface: PbrSurface, view_direction: vec3f, diffuse_radiance: vec3f, specular_radiance: vec3f) -> vec3f {
    let n_dot_v = saturate(dot(surface.normal, view_direction));
    let f0 = mix(vec3f(0.04), surface.base_color, surface.metallic);
    let diffuse = diffuse_radiance * surface.base_color * (1.0 - surface.metallic);
    let specular = specular_radiance * environment_brdf_approximation(f0, surface.roughness, n_dot_v);
    return (diffuse + specular) * surface.occlusion;
}
