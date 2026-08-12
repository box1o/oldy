fn bloom_threshold(color: vec3f, threshold: f32, knee: f32) -> vec3f {
    let brightness = max(max(color.r, color.g), color.b);
    let soft = clamp(brightness - threshold + knee, 0.0, 2.0 * knee);
    let contribution = max(brightness - threshold, soft * soft / max(4.0 * knee, 0.000001));
    return color * contribution / max(brightness, 0.000001);
}
fn bloom_composite(scene: vec3f, bloom: vec3f, intensity: f32) -> vec3f { return scene + bloom * intensity; }
