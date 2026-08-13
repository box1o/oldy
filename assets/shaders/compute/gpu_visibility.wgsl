struct Candidate {
    sphere: vec4f,
    bounds_extent_lod_scale: vec4f,
    masks: vec4u,
    lod_meshlets: vec4u,
    bin: vec4u,
    flags: vec4u,
}
struct LodRecord { geometric_error: f32, index_count: u32, first_index: u32, base_vertex: i32, lod: u32, padding0: u32, padding1: u32, padding2: u32 }
struct DrawIndexedCommand { index_count: u32, instance_count: u32, first_index: u32, base_vertex: i32, first_instance: u32 }
struct VisibilityParams {
    frustum_planes: array<vec4f, 6>,
    view_projection: mat4x4f,
    camera_position_threshold: vec4f,
    dimensions_flags: vec4u,
    counts: vec4u,
}
@group(0) @binding(0) var<uniform> params: VisibilityParams;
@group(0) @binding(1) var<storage, read> candidates: array<Candidate>;
@group(0) @binding(2) var<storage, read> lods: array<LodRecord>;
// Two vec4 values per meshlet: sphere then cone axis/cutoff.
@group(0) @binding(3) var<storage, read> meshlet_bounds: array<vec4f>;
@group(0) @binding(4) var previous_hiz: texture_2d<f32>;
@group(0) @binding(5) var<storage, read_write> commands: array<DrawIndexedCommand>;
@group(0) @binding(6) var<storage, read_write> bin_counts: array<atomic<u32>>;
@group(0) @binding(7) var<storage, read_write> visible_instances: array<vec2u>;
@group(0) @binding(8) var<storage, read_write> visible_meshlets: array<u32>;
@group(0) @binding(9) var<storage, read_write> diagnostics: array<atomic<u32>>;

fn outside_frustum(sphere: vec4f) -> bool {
    for (var plane = 0u; plane < 6u; plane += 1u) {
        if (dot(params.frustum_planes[plane].xyz, sphere.xyz) + params.frustum_planes[plane].w < -sphere.w) { return true; }
    }
    return false;
}

fn conservatively_occluded(candidate: Candidate) -> bool {
    let sphere = candidate.sphere;
    if ((params.dimensions_flags.w & 1u) == 0u) { return false; }
    let clip = params.view_projection * vec4f(sphere.xyz, 1.0);
    if (clip.w <= sphere.w || clip.w <= 0.0) { return false; }
    let ndc = clip.xyz / clip.w;
    let radius_pixels = sphere.w * f32(max(params.dimensions_flags.x, params.dimensions_flags.y)) / max(clip.w, 0.0001);
    let mip = min(u32(max(ceil(log2(max(radius_pixels * 2.0, 1.0))), 0.0)), params.dimensions_flags.z - 1u);
    let uv = clamp(ndc.xy * vec2f(0.5, -0.5) + 0.5, vec2f(0.0), vec2f(1.0));
    let size = vec2f(textureDimensions(previous_hiz, i32(mip)));
    let extent = vec2f(radius_pixels) / exp2(f32(mip));
    let minimum = vec2i(clamp(uv * size - extent, vec2f(0.0), size - 1.0));
    let maximum = vec2i(clamp(uv * size + extent, vec2f(0.0), size - 1.0));
    let farthest = max(max(textureLoad(previous_hiz, minimum, i32(mip)).x, textureLoad(previous_hiz, vec2i(maximum.x, minimum.y), i32(mip)).x),
                       max(textureLoad(previous_hiz, vec2i(minimum.x, maximum.y), i32(mip)).x, textureLoad(previous_hiz, maximum, i32(mip)).x));
    // Radius and bias both expand toward the camera. Ambiguous cases stay visible.
    let nearest_depth = candidate.bounds_extent_lod_scale.y - 0.002;
    return nearest_depth > farthest;
}

@compute @workgroup_size(64)
fn cull_and_classify(@builtin(global_invocation_id) id: vec3u) {
    if (id.x >= params.counts.x) { return; }
    atomicAdd(&diagnostics[0], 1u);
    let candidate = candidates[id.x];
    if (((candidate.masks.x & candidate.masks.z) | (candidate.masks.y & candidate.masks.w)) == 0u) { atomicAdd(&diagnostics[1], 1u); return; }
    if (outside_frustum(candidate.sphere)) { atomicAdd(&diagnostics[2], 1u); return; }
    if (candidate.flags.x != 1u && conservatively_occluded(candidate)) { atomicAdd(&diagnostics[3], 1u); return; }

    let object_distance = max(distance(candidate.sphere.xyz, params.camera_position_threshold.xyz) - candidate.sphere.w, 0.001);
    let projected_scale = candidate.bounds_extent_lod_scale.w * f32(params.dimensions_flags.y) / object_distance;
    var selected = candidate.lod_meshlets.x + candidate.lod_meshlets.y - 1u;
    for (var level = 0u; level < candidate.lod_meshlets.y; level += 1u) {
        let index = candidate.lod_meshlets.x + level;
        if (lods[index].geometric_error * projected_scale <= params.camera_position_threshold.w) { selected = index; break; }
    }
    let slot = atomicAdd(&bin_counts[candidate.bin.z], 1u);
    if (slot >= candidate.bin.y || candidate.bin.x + slot >= params.counts.y) { atomicAdd(&diagnostics[10], 1u); return; }
    let lod = lods[selected];
    if (lod.lod != bitcast<u32>(candidate.bounds_extent_lod_scale.x)) { atomicAdd(&diagnostics[5], 1u); }
    commands[candidate.bin.x + slot] = DrawIndexedCommand(lod.index_count, 1u, lod.first_index, lod.base_vertex, candidate.bin.w);
    visible_instances[candidate.bin.x + slot] = vec2u(bitcast<u32>(candidate.bounds_extent_lod_scale.z), lod.lod);
    atomicAdd(&diagnostics[4], 1u);
    atomicAdd(&diagnostics[9], 1u);

    for (var local = 0u; local < candidate.lod_meshlets.w; local += 1u) {
        let meshlet = candidate.lod_meshlets.z + local;
        if (meshlet >= params.counts.z) { break; }
        atomicAdd(&diagnostics[6], 1u);
        let bounds = meshlet_bounds[meshlet * 2u];
        let cone = meshlet_bounds[meshlet * 2u + 1u];
        let center = candidate.sphere.xyz + bounds.xyz;
        let view_distance = distance(params.camera_position_threshold.xyz, center);
        let view = normalize(params.camera_position_threshold.xyz - center);
        let angular_margin = min(1.0, bounds.w / max(view_distance - bounds.w, 0.0001));
        if (dot(cone.xyz, view) - angular_margin >= cone.w) { atomicAdd(&diagnostics[7], 1u); continue; }
        if (outside_frustum(vec4f(center, bounds.w))) { atomicAdd(&diagnostics[8], 1u); continue; }
        let output_index = atomicAdd(&diagnostics[11], 1u);
        if (output_index < params.counts.w) { visible_meshlets[output_index] = meshlet; } else { atomicAdd(&diagnostics[12], 1u); }
    }
}
