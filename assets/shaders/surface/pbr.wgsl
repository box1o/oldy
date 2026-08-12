struct PbrSurface {
    base_color: vec3f,
    metallic: f32,
    roughness: f32,
    normal: vec3f,
    emissive: vec3f,
    occlusion: f32,
};

fn fresnel_schlick(cos_theta: f32, f0: vec3f) -> vec3f { return f0 + (vec3f(1.0) - f0) * pow5(1.0 - saturate(cos_theta)); }
fn distribution_ggx(n_dot_h: f32, roughness: f32) -> f32 {
    let alpha = roughness * roughness;
    let alpha2 = alpha * alpha;
    let denominator = n_dot_h * n_dot_h * (alpha2 - 1.0) + 1.0;
    return alpha2 / max(WOKI_PI * denominator * denominator, WOKI_EPSILON);
}
fn visibility_smith_ggx_correlated(n_dot_v: f32, n_dot_l: f32, roughness: f32) -> f32 {
    let alpha2 = roughness * roughness * roughness * roughness;
    let gv = n_dot_l * safe_sqrt(n_dot_v * n_dot_v * (1.0 - alpha2) + alpha2);
    let gl = n_dot_v * safe_sqrt(n_dot_l * n_dot_l * (1.0 - alpha2) + alpha2);
    return 0.5 / max(gv + gl, WOKI_EPSILON);
}
fn evaluate_pbr_brdf(surface: PbrSurface, view_direction: vec3f, light_direction: vec3f) -> vec3f {
    let half_vector = safe_normalize3(view_direction + light_direction);
    let n_dot_v = saturate(dot(surface.normal, view_direction));
    let n_dot_l = saturate(dot(surface.normal, light_direction));
    let n_dot_h = saturate(dot(surface.normal, half_vector));
    let v_dot_h = saturate(dot(view_direction, half_vector));
    let dielectric_f0 = vec3f(0.04);
    let f0 = mix(dielectric_f0, surface.base_color, surface.metallic);
    let fresnel = fresnel_schlick(v_dot_h, f0);
    let specular = distribution_ggx(n_dot_h, max(surface.roughness, 0.045)) * visibility_smith_ggx_correlated(n_dot_v, n_dot_l, max(surface.roughness, 0.045)) * fresnel;
    let diffuse = (vec3f(1.0) - fresnel) * (1.0 - surface.metallic) * surface.base_color / WOKI_PI;
    return (diffuse + specular) * n_dot_l;
}
