fn global_linear_index(global_id: vec3u, dimensions: vec3u) -> u32 {
    return global_id.x + dimensions.x * (global_id.y + dimensions.y * global_id.z);
}

fn local_linear_index(local_id: vec3u, workgroup_size: vec3u) -> u32 {
    return local_id.x + workgroup_size.x * (local_id.y + workgroup_size.y * local_id.z);
}

fn dispatch_in_bounds(global_id: vec3u, dimensions: vec3u) -> bool {
    return all(global_id < dimensions);
}
