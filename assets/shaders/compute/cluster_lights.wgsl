@group(0) @binding(0) var<uniform> params: ClusterParams;
@group(0) @binding(1) var<storage, read> lights: array<GpuLightRecord>;
@group(0) @binding(2) var<storage, read_write> clusters: array<ClusterGridRecord>;
@group(0) @binding(3) var<storage, read_write> indices: array<u32>;
@group(0) @binding(4) var<storage, read_write> overflow_total: atomic<u32>;

@compute @workgroup_size(64)
fn assign_clusters(@builtin(workgroup_id) workgroup: vec3u, @builtin(local_invocation_index) lane: u32) {
    let cluster_index = workgroup.x;
    if (cluster_index >= params.dimensions.x * params.dimensions.y * params.dimensions.z || lane != 0u) { return; }
    let x = cluster_index % params.dimensions.x;
    let yz = cluster_index / params.dimensions.x;
    let y = yz % params.dimensions.y;
    let z = yz / params.dimensions.y;
    let z0 = params.depth.x * exp(params.depth.z * f32(z) / f32(params.dimensions.z));
    let z1 = params.depth.x * exp(params.depth.z * f32(z + 1u) / f32(params.dimensions.z));
    let minimum = vec2f(2.0 * f32(x) / f32(params.dimensions.x) - 1.0, 2.0 * f32(y) / f32(params.dimensions.y) - 1.0);
    let maximum = vec2f(2.0 * f32(x + 1u) / f32(params.dimensions.x) - 1.0, 2.0 * f32(y + 1u) / f32(params.dimensions.y) - 1.0);
    var record = ClusterGridRecord(cluster_index * params.dimensions.w, 0u, 0u, 0u);
    for (var light_index = 0u; light_index < u32(params.depth.w); light_index += 1u) {
        let light = lights[light_index];
        let p = light.position_range.xyz;
        let view_position = vec3f(dot(params.view_row_0.xyz, p) + params.view_row_0.w, dot(params.view_row_1.xyz, p) + params.view_row_1.w, -(dot(params.view_row_2.xyz, p) + params.view_row_2.w));
        let radius = light.position_range.w;
        if (view_position.z + radius < z0 || view_position.z - radius > z1 || view_position.z + radius <= 0.0) { continue; }
        let ndc = view_position.xy * params.projection.xy / max(view_position.z, 0.001);
        let projected_radius = radius * params.projection.xy / max(view_position.z - radius, 0.001);
        if (ndc.x + projected_radius.x < minimum.x || ndc.x - projected_radius.x > maximum.x || ndc.y + projected_radius.y < minimum.y || ndc.y - projected_radius.y > maximum.y) { continue; }
        if (record.count < params.dimensions.w) {
            indices[record.offset + record.count] = light_index;
            record.count += 1u;
        } else { record.overflow += 1u; }
    }
    if (record.overflow != 0u) { atomicAdd(&overflow_total, record.overflow); }
    clusters[cluster_index] = record;
}
