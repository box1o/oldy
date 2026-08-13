fn skin_matrix(joints: vec4u, weights: vec4f) -> mat4x4f {
    let normalized_weights = weights / max(dot(weights, vec4f(1.0)), 0.000001);
    return joint_matrices[joints.x] * normalized_weights.x
        + joint_matrices[joints.y] * normalized_weights.y
        + joint_matrices[joints.z] * normalized_weights.z
        + joint_matrices[joints.w] * normalized_weights.w;
}

// Previous-pose palettes use the same weighted convention. The physical binding
// aliases current pose until an animation system publishes a previous palette,
// which is the required zero-velocity fallback for newly spawned skinned meshes.
fn previous_skin_matrix(joints: vec4u, weights: vec4f) -> mat4x4f {
    let normalized_weights = weights / max(dot(weights, vec4f(1.0)), 0.000001);
    return previous_joint_matrices[joints.x] * normalized_weights.x
        + previous_joint_matrices[joints.y] * normalized_weights.y
        + previous_joint_matrices[joints.z] * normalized_weights.z
        + previous_joint_matrices[joints.w] * normalized_weights.w;
}
