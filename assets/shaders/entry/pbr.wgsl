fn pbr_vertex(
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

fn pbr_gpu_vertex(position: vec3f, normal: vec3f, tangent: vec4f, uv: vec2f, color: vec4f, instance: u32) -> MeshVaryings {
    let model = gpu_instance_model(instance);
    let world_position = transform_point(model, position);
    let world_normal = safe_normalize3(transform_vector(gpu_instance_normal(model), normal));
    let world_tangent = vec4f(safe_normalize3(transform_vector(model, tangent.xyz)), tangent.w);
    return MeshVaryings(view.view_projection * vec4f(world_position, 1.0), world_position, world_normal, world_tangent, uv, color);
}

@vertex
fn pbr_gpu_vs(input: StaticVertex, @builtin(instance_index) instance: u32) -> MeshVaryings {
    return pbr_gpu_vertex(input.position, input.normal, input.tangent, input.uv, input.color, instance);
}

@vertex
fn pbr_static_vs(input: StaticVertex) -> MeshVaryings {
    return pbr_vertex(
        input.position,
        input.normal,
        input.tangent,
        input.uv,
        input.color,
        identity_matrix()
    );
}

@vertex
fn pbr_skinned_vs(input: SkinnedVertex) -> MeshVaryings {
    return pbr_vertex(
        input.position,
        input.normal,
        input.tangent,
        input.uv,
        input.color,
        skin_matrix(input.joints, input.weights)
    );
}

fn pbr_velocity_vertex(position: vec3f, current_local: mat4x4f, previous_local: mat4x4f) -> VelocityVaryings {
    let current_world = object.model * current_local * vec4f(position, 1.0);
    let previous_world = object.previous_model * previous_local * vec4f(position, 1.0);
    let current_clip = view.view_projection * current_world;
    let previous_clip = view.previous_view_projection * previous_world;
    return VelocityVaryings(current_clip, current_clip, previous_clip);
}

@vertex
fn pbr_velocity_static_vs(input: StaticVertex) -> VelocityVaryings {
    return pbr_velocity_vertex(input.position, identity_matrix(), identity_matrix());
}

@vertex
fn pbr_velocity_skinned_vs(input: SkinnedVertex) -> VelocityVaryings {
    return pbr_velocity_vertex(
        input.position,
        skin_matrix(input.joints, input.weights),
        previous_skin_matrix(input.joints, input.weights)
    );
}

@fragment
fn pbr_velocity_fs(input: VelocityVaryings) -> @location(0) vec4f {
    return vec4f(clip_velocity(input.current_clip, input.previous_clip), 0.0, 1.0);
}

@fragment
fn pbr_fs(input: MeshVaryings) -> @location(0) vec4f {
    let base_sample = textureSample(base_color_texture, material_sampler, input.uv)
        * material.base_color_factor
        * input.color;
    if (base_sample.a < material.alpha_cutoff) {
        discard;
    }
    let mr = textureSample(metallic_roughness_texture, material_sampler, input.uv);
    let mapped_normal = apply_normal_map(
        input.world_normal,
        input.world_tangent,
        textureSample(normal_texture, material_sampler, input.uv).xyz,
        material.normal_scale
    );
    let surface = PbrSurface(
        base_sample.rgb,
        saturate(mr.b * material.metallic_factor),
        clamp(mr.g * material.roughness_factor, 0.045, 1.0),
        mapped_normal,
        textureSample(emissive_texture, material_sampler, input.uv).rgb
            * material.emissive_factor,
        mix(
            1.0,
            textureSample(occlusion_texture, material_sampler, input.uv).r,
            material.occlusion_strength
        )
    );
    let view_direction = safe_normalize3(view.camera_position - input.world_position);
    let camera_relative = input.world_position - view.camera_position;
    let view_depth = max(-dot(cluster_params.view_row_2.xyz, camera_relative), cluster_params.depth.x);
    var color = sample_environment_ibl(surface, view_direction) + surface.emissive;
    for (var i = 0u; i < scene.directional_count; i += 1u) {
        let light = evaluate_light(scene.directional_lights[i], input.world_position);
        var visibility = 1.0;
        if (i == 0u && shadow_data.atlas_size_count.z > 0.0) {
            let cascade = select_shadow_cascade(view_depth, shadow_data.split_depths, u32(shadow_data.atlas_size_count.z));
            let shadow_clip = shadow_data.matrices[cascade] * vec4f(input.world_position, 1.0);
            let shadow_ndc = shadow_clip.xyz / shadow_clip.w;
            let receiver_bias = shadow_receiver_bias(surface.normal, light.direction, shadow_data.atlas_size_count.xy);
            visibility = sample_shadow_pcf(shadow_atlas, shadow_sampler, shadow_ndc.xy * vec2f(0.5, -0.5) + 0.5, i32(cascade), shadow_ndc.z - receiver_bias,
                vec2f(shadow_data.atlas_size_count.xy));
        }
        color += evaluate_pbr_brdf(surface, view_direction, light.direction) * light.radiance * visibility;
    }
    let tile_x = min(u32(input.clip_position.x * f32(cluster_params.dimensions.x) / cluster_params.projection.z), cluster_params.dimensions.x - 1u);
    let tile_y = cluster_params.dimensions.y - 1u - min(u32(input.clip_position.y * f32(cluster_params.dimensions.y) / cluster_params.projection.w), cluster_params.dimensions.y - 1u);
    let slice = min(u32(log(view_depth / cluster_params.depth.x) / cluster_params.depth.z * f32(cluster_params.dimensions.z)), cluster_params.dimensions.z - 1u);
    let cluster = cluster_grid[(slice * cluster_params.dimensions.y + tile_y) * cluster_params.dimensions.x + tile_x];
    for (var i = 0u; i < cluster.count; i += 1u) {
        let light = evaluate_light(local_lights[cluster_light_indices[cluster.offset + i]], input.world_position);
        color += evaluate_pbr_brdf(surface, view_direction, light.direction) * light.radiance;
    }
    return vec4f(color, base_sample.a);
}
