struct ObjectData { model: mat4x4f, normal_matrix: mat4x4f };
@group(3) @binding(0) var<uniform> object: ObjectData;
@group(3) @binding(1) var<storage, read> joint_matrices: array<mat4x4f>;
