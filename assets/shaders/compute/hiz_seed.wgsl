struct HiZParams { dimensions: vec2u, _padding: vec2u }
@group(0) @binding(0) var<uniform> params: HiZParams;
@group(0) @binding(1) var source_depth: texture_depth_2d;
@group(0) @binding(2) var destination: texture_storage_2d<r32float, write>;

@compute @workgroup_size(8, 8)
fn hiz_seed(@builtin(global_invocation_id) id: vec3u) {
    if (any(id.xy >= params.dimensions)) { return; }
    textureStore(destination, vec2i(id.xy), vec4f(textureLoad(source_depth, vec2i(id.xy), 0)));
}
