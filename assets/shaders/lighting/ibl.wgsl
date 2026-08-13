fn environment_brdf_approximation(f0: vec3f, roughness: f32, n_dot_v: f32) -> vec3f {
    let r = roughness * vec4f(-1.0, -0.0275, -0.572, 0.022)
        + vec4f(1.0, 0.0425, 1.04, -0.04);
    let a004 = min(r.x * r.x, exp2(-9.28 * n_dot_v)) * r.x + r.y;
    let ab = vec2f(-1.04, 1.04) * a004 + r.zw;
    return f0 * ab.x + vec3f(ab.y);
}

fn evaluate_ibl(
    surface: PbrSurface,
    view_direction: vec3f,
    diffuse_radiance: vec3f,
    specular_radiance: vec3f
) -> vec3f {
    let n_dot_v = saturate(dot(surface.normal, view_direction));
    let f0 = mix(vec3f(0.04), surface.base_color, surface.metallic);
    let diffuse = diffuse_radiance * surface.base_color * (1.0 - surface.metallic);
    let specular = specular_radiance
        * environment_brdf_approximation(f0, surface.roughness, n_dot_v);
    return (diffuse + specular) * surface.occlusion;
}

fn sample_environment_ibl(surface: PbrSurface, view_direction: vec3f) -> vec3f {
    if (environment_data.lighting_enabled == 0u) {
        return vec3f(0.0);
    }
    let n_dot_v = saturate(dot(surface.normal, view_direction));
    let f0 = mix(vec3f(0.04), surface.base_color, surface.metallic);
    let reflection = reflect(-view_direction, surface.normal);
    let diffuse = textureSample(environment_irradiance, environment_sampler, surface.normal).rgb
        * surface.base_color * (1.0 - surface.metallic);
    let prefiltered = textureSampleLevel(environment_prefiltered, environment_sampler, reflection,
        surface.roughness * max(environment_data.specular_mip_count - 1.0, 0.0)).rgb;
    let brdf = textureSample(environment_brdf_lut, environment_sampler, vec2f(n_dot_v, surface.roughness)).rg;
    return (diffuse + prefiltered * (f0 * brdf.x + brdf.y)) * surface.occlusion;
}
