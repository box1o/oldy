@vertex
fn cube_vs(input: StaticVertex) -> MeshVaryings {
    let world_position = transform_point(object.model, input.position);
    return MeshVaryings(
        view.view_projection * vec4f(world_position, 1.0),
        world_position,
        safe_normalize3(transform_vector(object.normal_matrix, input.normal)),
        input.tangent,
        input.uv,
        input.color
    );
}

@vertex
fn cube_instanced_vs(input: StaticVertex, instance: InstanceData) -> MeshVaryings {
    let model = object.model * instance_matrix(
        instance.model_column0,
        instance.model_column1,
        instance.model_column2,
        instance.model_column3
    );
    let world_position = transform_point(model, input.position);
    return MeshVaryings(
        view.view_projection * vec4f(world_position, 1.0),
        world_position,
        safe_normalize3(transform_vector(model, input.normal)),
        input.tangent,
        input.uv,
        input.color
    );
}

@fragment
fn cube_fs(input: MeshVaryings) -> @location(0) vec4f {
    return input.color;
}
