struct StaticVertex {
    @location(0) position: vec3f,
    @location(1) normal: vec3f,
    @location(2) tangent: vec4f,
    @location(3) uv: vec2f,
    @location(4) color: vec4f,
};
struct SkinnedVertex {
    @location(0) position: vec3f,
    @location(1) normal: vec3f,
    @location(2) tangent: vec4f,
    @location(3) uv: vec2f,
    @location(4) color: vec4f,
    @location(5) joints: vec4u,
    @location(6) weights: vec4f,
};
struct InstanceData {
    @location(7) model_column0: vec4f,
    @location(8) model_column1: vec4f,
    @location(9) model_column2: vec4f,
    @location(10) model_column3: vec4f,
};
struct MeshVaryings {
    @builtin(position) clip_position: vec4f,
    @location(0) world_position: vec3f,
    @location(1) world_normal: vec3f,
    @location(2) world_tangent: vec4f,
    @location(3) uv: vec2f,
    @location(4) color: vec4f,
};

fn instance_matrix(column0: vec4f, column1: vec4f, column2: vec4f, column3: vec4f) -> mat4x4f {
    return mat4x4f(column0, column1, column2, column3);
}
