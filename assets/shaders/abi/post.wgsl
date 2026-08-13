@group(2) @binding(0) var<uniform> post: PostData;
@group(2) @binding(1) var source_texture: texture_2d<f32>;
@group(2) @binding(2) var source_sampler: sampler;
@group(2) @binding(3) var bloom_texture: texture_2d<f32>;
@group(2) @binding(4) var depth_texture: texture_depth_2d;
@group(2) @binding(5) var velocity_texture: texture_2d<f32>;
@group(2) @binding(6) var grading_lut: texture_3d<f32>;
