fn skin_matrix(joints: vec4u, weights: vec4f) -> mat4x4f {
    let normalized_weights = weights / max(dot(weights, vec4f(1.0)), 0.000001);
    return joint_matrices[joints.x] * normalized_weights.x + joint_matrices[joints.y] * normalized_weights.y + joint_matrices[joints.z] * normalized_weights.z + joint_matrices[joints.w] * normalized_weights.w;
}
