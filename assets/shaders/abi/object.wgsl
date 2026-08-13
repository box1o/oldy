@group(3) @binding(0) var<uniform> object: ObjectData;
@group(3) @binding(1) var<storage, read> joint_matrices: array<mat4x4f>;
@group(3) @binding(2) var<storage, read> previous_joint_matrices: array<mat4x4f>;
@group(3) @binding(3) var<storage, read> gpu_instances: array<GpuInstanceRecord>;

fn gpu_instance_model(index: u32) -> mat4x4f {
    let packed = gpu_instances[index].current_transform;
    return mat4x4f(
        vec4f(packed[0].x, packed[1].x, packed[2].x, 0.0),
        vec4f(packed[0].y, packed[1].y, packed[2].y, 0.0),
        vec4f(packed[0].z, packed[1].z, packed[2].z, 0.0),
        vec4f(packed[0].w, packed[1].w, packed[2].w, 1.0)
    );
}

fn gpu_instance_normal(model: mat4x4f) -> mat4x4f {
    let a = model[0].xyz;
    let b = model[1].xyz;
    let c = model[2].xyz;
    let raw_determinant = dot(a, cross(b, c));
    let determinant = select(-max(abs(raw_determinant), 0.000001), max(abs(raw_determinant), 0.000001), raw_determinant >= 0.0);
    return mat4x4f(vec4f(cross(b, c) / determinant, 0.0), vec4f(cross(c, a) / determinant, 0.0), vec4f(cross(a, b) / determinant, 0.0), vec4f(0.0, 0.0, 0.0, 1.0));
}
