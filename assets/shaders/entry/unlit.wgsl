fn unlit_vertex(position: vec3f, normal: vec3f, tangent: vec4f, uv: vec2f, color: vec4f, local_matrix: mat4x4f) -> MeshVaryings {
    let model = object.model * local_matrix;
    let world_position = transform_point(model, position);
    let world_normal = safe_normalize3(transform_vector(object.normal_matrix * local_matrix, normal));
    let world_tangent = vec4f(safe_normalize3(transform_vector(model, tangent.xyz)), tangent.w);
    return MeshVaryings(view.view_projection * vec4f(world_position, 1.0), world_position, world_normal, world_tangent, uv, color);
}
@vertex fn unlit_static_vs(input: StaticVertex) -> MeshVaryings { return unlit_vertex(input.position, input.normal, input.tangent, input.uv, input.color, identity_matrix()); }
@vertex fn unlit_skinned_vs(input: SkinnedVertex) -> MeshVaryings { return unlit_vertex(input.position, input.normal, input.tangent, input.uv, input.color, skin_matrix(input.joints, input.weights)); }
@fragment fn unlit_fs(input: MeshVaryings) -> @location(0) vec4f {
    let sampled = textureSample(base_color_texture, material_sampler, input.uv);
    let color = evaluate_unlit(material.base_color * sampled, input.color);
    if (color.a < material.alpha_cutoff) { discard; }
    return color;
}
