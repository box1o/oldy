struct SkyOutput { @builtin(position) position: vec4f, @location(0) direction: vec3f };
@vertex fn sky_vs(@builtin(vertex_index) vertex_index: u32) -> SkyOutput {
    let output = fullscreen_vertex(vertex_index);
    let world = view.inverse_view_projection * vec4f(uv_to_ndc(output.uv), 1.0, 1.0);
    return SkyOutput(output.position, safe_normalize3(world.xyz / world.w - view.camera_position));
}
@fragment fn sky_fs(input: SkyOutput) -> @location(0) vec4f {
    let horizon = vec3f(0.35, 0.5, 0.75);
    let zenith = vec3f(0.04, 0.12, 0.3);
    return vec4f(mix(horizon, zenith, saturate(input.direction.y)), 1.0);
}
