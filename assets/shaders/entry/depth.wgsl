struct DepthOutput {
    @builtin(position) position: vec4f
};

fn depth_vertex(position: vec3f, local_matrix: mat4x4f) -> DepthOutput {
    return DepthOutput(view.view_projection * object.model * local_matrix * vec4f(position, 1.0));
}

@vertex
fn depth_static_vs(input: StaticVertex) -> DepthOutput {
    return depth_vertex(input.position, identity_matrix());
}

@vertex
fn depth_gpu_vs(input: StaticVertex, @builtin(instance_index) instance: u32) -> DepthOutput {
    return DepthOutput(view.view_projection * gpu_instance_model(instance) * vec4f(input.position, 1.0));
}

@vertex
fn depth_skinned_vs(input: SkinnedVertex) -> DepthOutput {
    return depth_vertex(input.position, skin_matrix(input.joints, input.weights));
}
