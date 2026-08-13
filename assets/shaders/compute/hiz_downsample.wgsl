struct HiZParams { dimensions: vec2u, _padding: vec2u }
@group(0) @binding(0) var<uniform> params: HiZParams;
@group(0) @binding(1) var source_mip: texture_2d<f32>;
@group(0) @binding(2) var destination: texture_storage_2d<r32float, write>;

@compute @workgroup_size(8, 8)
fn hiz_downsample(@builtin(global_invocation_id) id: vec3u) {
    if (any(id.xy >= params.dimensions)) { return; }
    let source_size = textureDimensions(source_mip);
    let p = vec2i(id.xy * 2u);
    let p1 = min(p + vec2i(1, 0), vec2i(source_size) - 1);
    let p2 = min(p + vec2i(0, 1), vec2i(source_size) - 1);
    let p3 = min(p + vec2i(1, 1), vec2i(source_size) - 1);
    let farthest = max(max(textureLoad(source_mip, p, 0).x, textureLoad(source_mip, p1, 0).x), max(textureLoad(source_mip, p2, 0).x, textureLoad(source_mip, p3, 0).x));
    textureStore(destination, vec2i(id.xy), vec4f(farthest));
}
