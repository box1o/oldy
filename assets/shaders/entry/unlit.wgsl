fn unlit_vertex(
    position: vec3f,
    normal: vec3f,
    tangent: vec4f,
    uv: vec2f,
    color: vec4f,
    local_matrix: mat4x4f
) -> MeshVaryings {
    let model = object.model * local_matrix;
    let world_position = transform_point(model, position);
    let world_normal = safe_normalize3(
        transform_vector(object.normal_matrix * local_matrix, normal)
    );
    let world_tangent = vec4f(
        safe_normalize3(transform_vector(model, tangent.xyz)),
        tangent.w
    );
    return MeshVaryings(
        view.view_projection * vec4f(world_position, 1.0),
        world_position,
        world_normal,
        world_tangent,
        uv,
        color
    );
}

@vertex
fn unlit_gpu_vs(input: StaticVertex, @builtin(instance_index) instance: u32) -> MeshVaryings {
    let model = gpu_instance_model(instance);
    let world_position = transform_point(model, input.position);
    let world_normal = safe_normalize3(transform_vector(gpu_instance_normal(model), input.normal));
    let world_tangent = vec4f(safe_normalize3(transform_vector(model, input.tangent.xyz)), input.tangent.w);
    return MeshVaryings(view.view_projection * vec4f(world_position, 1.0), world_position, world_normal, world_tangent, input.uv, input.color);
}

fn unlit_velocity_vertex(position: vec3f, current_local: mat4x4f, previous_local: mat4x4f) -> VelocityVaryings {
    let current_clip = view.view_projection * object.model * current_local * vec4f(position, 1.0);
    let previous_clip = view.previous_view_projection * object.previous_model * previous_local * vec4f(position, 1.0);
    return VelocityVaryings(current_clip, current_clip, previous_clip);
}

@vertex
fn unlit_velocity_static_vs(input: StaticVertex) -> VelocityVaryings {
    return unlit_velocity_vertex(input.position, identity_matrix(), identity_matrix());
}

@vertex
fn unlit_velocity_skinned_vs(input: SkinnedVertex) -> VelocityVaryings {
    return unlit_velocity_vertex(input.position, skin_matrix(input.joints, input.weights), previous_skin_matrix(input.joints, input.weights));
}

@fragment
fn unlit_velocity_fs(input: VelocityVaryings) -> @location(0) vec2f {
    let current_ndc = input.current_clip.xy / max(input.current_clip.w, 0.000001);
    let previous_ndc = input.previous_clip.xy / max(input.previous_clip.w, 0.000001);
    return (current_ndc - previous_ndc) * vec2f(0.5, -0.5);
}

@vertex
fn unlit_static_vs(input: StaticVertex) -> MeshVaryings {
    return unlit_vertex(
        input.position,
        input.normal,
        input.tangent,
        input.uv,
        input.color,
        identity_matrix()
    );
}

@vertex
fn unlit_skinned_vs(input: SkinnedVertex) -> MeshVaryings {
    return unlit_vertex(
        input.position,
        input.normal,
        input.tangent,
        input.uv,
        input.color,
        skin_matrix(input.joints, input.weights)
    );
}

@fragment
fn unlit_fs(input: MeshVaryings) -> @location(0) vec4f {
    let sampled = textureSample(base_color_texture, material_sampler, input.uv);
    let color = evaluate_unlit(material.base_color * sampled, input.color);
    if (color.a < material.alpha_cutoff) {
        discard;
    }
    return color;
}
