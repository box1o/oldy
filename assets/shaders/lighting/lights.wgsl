const LIGHT_DIRECTIONAL: u32 = 0u;
const LIGHT_POINT: u32 = 1u;
const LIGHT_SPOT: u32 = 2u;

struct Light {
    position_range: vec4f,
    direction_outer: vec4f,
    color_intensity: vec4f,
    kind_inner: vec4u,
};
struct LightSample { direction: vec3f, radiance: vec3f };

fn distance_attenuation(distance: f32, range: f32) -> f32 {
    let normalized = distance / max(range, 0.000001);
    let window = saturate(1.0 - normalized * normalized * normalized * normalized);
    return window * window / max(distance * distance, 0.01);
}
fn evaluate_light(light: Light, world_position: vec3f) -> LightSample {
    if (light.kind_inner.x == LIGHT_DIRECTIONAL) {
        return LightSample(safe_normalize3(-light.direction_outer.xyz), light.color_intensity.rgb * light.color_intensity.a);
    }
    let offset = light.position_range.xyz - world_position;
    let distance = length(offset);
    let direction = offset / max(distance, 0.000001);
    var attenuation = distance_attenuation(distance, light.position_range.w);
    if (light.kind_inner.x == LIGHT_SPOT) {
        let cosine = dot(-direction, safe_normalize3(light.direction_outer.xyz));
        let inner = bitcast<f32>(light.kind_inner.y);
        attenuation *= smoothstep(light.direction_outer.w, inner, cosine);
    }
    return LightSample(direction, light.color_intensity.rgb * light.color_intensity.a * attenuation);
}
