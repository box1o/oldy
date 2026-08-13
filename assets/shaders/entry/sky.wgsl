struct SkyOutput {
    @builtin(position) position: vec4f,
    @location(0) direction: vec3f
};

@group(2) @binding(0) var sky_environment: texture_cube<f32>;
@group(2) @binding(1) var sky_sampler: sampler;
@group(2) @binding(2) var sky_depth: texture_depth_2d;

@vertex
fn sky_vs(@builtin(vertex_index) vertex_index: u32) -> SkyOutput {
    let full = fullscreen_vertex(vertex_index);
    let ndc_position = vec2f(full.uv.x * 2.0 - 1.0, 1.0 - full.uv.y * 2.0);
    return SkyOutput(
        full.position,
        safe_normalize3(vec3f(ndc_position, 1.0))
    );
}

@fragment
fn sky_fs(input: SkyOutput) -> @location(0) vec4f {
    if (textureLoad(sky_depth, vec2i(input.position.xy), 0) < 0.999999) {
        discard;
    }
    return vec4f(textureSampleLevel(sky_environment, sky_sampler, input.direction, 0.0).rgb, 1.0);
}
