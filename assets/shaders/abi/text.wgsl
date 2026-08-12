struct TextData { color: vec4f, parameters: vec4f };
@group(2) @binding(0) var<uniform> text_material: TextData;
@group(2) @binding(1) var glyph_atlas: texture_2d<f32>;
@group(2) @binding(2) var glyph_sampler: sampler;
