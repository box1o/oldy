struct FrameData { time: f32, delta_time: f32, exposure: f32, light_count: u32 };
struct SceneData { ambient_color: vec3f, _padding: f32, lights: array<Light, 16> };
@group(0) @binding(0) var<uniform> frame: FrameData;
@group(0) @binding(1) var<uniform> scene: SceneData;
