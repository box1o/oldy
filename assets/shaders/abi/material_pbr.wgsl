struct PbrMaterial {
    base_color_factor: vec4f,
    emissive_factor: vec3f,
    normal_scale: f32,
    metallic_factor: f32,
    roughness_factor: f32,
    occlusion_strength: f32,
    alpha_cutoff: f32,
};
@group(2) @binding(0) var<uniform> material: PbrMaterial;
@group(2) @binding(1) var base_color_texture: texture_2d<f32>;
@group(2) @binding(2) var material_sampler: sampler;
@group(2) @binding(3) var normal_texture: texture_2d<f32>;
@group(2) @binding(4) var metallic_roughness_texture: texture_2d<f32>;
@group(2) @binding(5) var emissive_texture: texture_2d<f32>;
@group(2) @binding(6) var occlusion_texture: texture_2d<f32>;
