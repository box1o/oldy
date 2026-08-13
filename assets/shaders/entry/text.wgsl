struct TextVertex {
    @location(0) position: vec2f,
    @location(1) uv: vec2f,
    @location(2) color: vec4f
};

struct TextVaryings {
    @builtin(position) position: vec4f,
    @location(0) uv: vec2f,
    @location(1) color: vec4f
};

@vertex
fn text_vs(input: TextVertex) -> TextVaryings {
    return TextVaryings(vec4f(input.position, 0.0, 1.0), input.uv, input.color);
}

@fragment
fn bitmap_text_fs(input: TextVaryings) -> @location(0) vec4f {
    let coverage = bitmap_coverage(
        textureSample(glyph_atlas, glyph_sampler, input.uv).r,
        text_material.parameters.x
    );
    return vec4f(
        text_material.color.rgb * input.color.rgb,
        text_material.color.a * input.color.a * coverage
    );
}

@fragment
fn sdf_text_fs(input: TextVaryings) -> @location(0) vec4f {
    let coverage = sdf_coverage(
        textureSample(glyph_atlas, glyph_sampler, input.uv).r,
        max(fwidth(input.uv.x) * text_material.parameters.x, 0.000001)
    );
    return vec4f(
        text_material.color.rgb * input.color.rgb,
        text_material.color.a * input.color.a * coverage
    );
}

@fragment
fn msdf_text_fs(input: TextVaryings) -> @location(0) vec4f {
    let coverage = msdf_coverage(
        textureSample(glyph_atlas, glyph_sampler, input.uv).rgb,
        max(fwidth(input.uv.x) * text_material.parameters.x, 0.000001)
    );
    return vec4f(
        text_material.color.rgb * input.color.rgb,
        text_material.color.a * input.color.a * coverage
    );
}
