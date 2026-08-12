fn pbr_vertex(position: vec3f, normal: vec3f, tangent: vec4f, uv: vec2f, color: vec4f, local_matrix: mat4x4f) -> MeshVaryings {
    let model = object.model * local_matrix;
    let world_position = transform_point(model, position);
    let world_normal = safe_normalize3(transform_vector(object.normal_matrix * local_matrix, normal));
    let world_tangent = vec4f(safe_normalize3(transform_vector(model, tangent.xyz)), tangent.w);
    return MeshVaryings(view.view_projection * vec4f(world_position, 1.0), world_position, world_normal, world_tangent, uv, color);
}
@vertex fn pbr_static_vs(input: StaticVertex) -> MeshVaryings { return pbr_vertex(input.position, input.normal, input.tangent, input.uv, input.color, identity_matrix()); }
@vertex fn pbr_skinned_vs(input: SkinnedVertex) -> MeshVaryings { return pbr_vertex(input.position, input.normal, input.tangent, input.uv, input.color, skin_matrix(input.joints, input.weights)); }
@fragment fn pbr_fs(input: MeshVaryings) -> @location(0) vec4f {
    let base_sample = textureSample(base_color_texture, material_sampler, input.uv) * material.base_color_factor * input.color;
    if (base_sample.a < material.alpha_cutoff) { discard; }
    let mr = textureSample(metallic_roughness_texture, material_sampler, input.uv);
    let mapped_normal = apply_normal_map(input.world_normal, input.world_tangent, textureSample(normal_texture, material_sampler, input.uv).xyz, material.normal_scale);
    let surface = PbrSurface(base_sample.rgb, saturate(mr.b * material.metallic_factor), clamp(mr.g * material.roughness_factor, 0.045, 1.0), mapped_normal, textureSample(emissive_texture, material_sampler, input.uv).rgb * material.emissive_factor, mix(1.0, textureSample(occlusion_texture, material_sampler, input.uv).r, material.occlusion_strength));
    let view_direction = safe_normalize3(view.camera_position - input.world_position);
    var color = surface.base_color * scene.ambient_color * surface.occlusion + surface.emissive;
    let light_count = min(frame.light_count, 16u);
    for (var i = 0u; i < light_count; i += 1u) {
        let light = evaluate_light(scene.lights[i], input.world_position);
        color += evaluate_pbr_brdf(surface, view_direction, light.direction) * light.radiance;
    }
    return vec4f(color, base_sample.a);
}
