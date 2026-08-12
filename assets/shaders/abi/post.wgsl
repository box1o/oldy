struct PostData { parameters: vec4f, texel_size: vec2f, _padding: vec2f };
@group(2) @binding(0) var<uniform> post: PostData;
@group(2) @binding(1) var source_texture: texture_2d<f32>;
@group(2) @binding(2) var source_sampler: sampler;
@group(2) @binding(3) var bloom_texture: texture_2d<f32>;
