struct CanvasFrame {
    viewport: vec2f,
    kind: u32,
    fit: u32,
    rect: vec4f,
    image_size: vec2f,
    target_srgb: u32,
    _padding: u32,
};

@group(0) @binding(0) var<uniform> canvas: CanvasFrame;
@group(1) @binding(0) var canvas_image: texture_2d<f32>;
@group(1) @binding(1) var canvas_sampler: sampler;

struct CanvasVertex {
    @location(0) position: vec2f,
    @location(1) uv: vec2f,
    @location(2) color: vec4f,
    @location(3) radii: vec4f,
    @location(4) extra: vec2f,
};

struct CanvasVaryings {
    @builtin(position) position: vec4f,
    @location(0) uv: vec2f,
    @location(1) color: vec4f,
    @location(2) radii: vec4f,
    @location(3) extra: vec2f,
};

@vertex
fn canvas_vs(input: CanvasVertex) -> CanvasVaryings {
    let ndc = vec2f(input.position.x / canvas.viewport.x * 2.0 - 1.0, 1.0 - input.position.y / canvas.viewport.y * 2.0);
    return CanvasVaryings(vec4f(ndc, 0.0, 1.0), input.uv, input.color, input.radii, input.extra);
}

fn rounded_distance(uv: vec2f, size: vec2f, radii: vec4f) -> f32 {
    let top = select(radii.x, radii.y, uv.x > 0.5);
    let bottom = select(radii.w, radii.z, uv.x > 0.5);
    let radius = max(select(top, bottom, uv.y > 0.5), 0.0);
    let point = abs(uv * size - size * 0.5) - size * 0.5 + vec2f(radius);
    return min(max(point.x, point.y), 0.0) + length(max(point, vec2f(0.0))) - radius;
}

fn image_uv(uv: vec2f) -> vec3f {
    if (canvas.fit == 0u || canvas.image_size.x <= 0.0 || canvas.image_size.y <= 0.0) {
        return vec3f(uv, 1.0);
    }
    let target_size = max(canvas.rect.zw, vec2f(1.0));
    let contain_scale = min(target_size.x / canvas.image_size.x, target_size.y / canvas.image_size.y);
    let cover_scale = max(target_size.x / canvas.image_size.x, target_size.y / canvas.image_size.y);
    var scale = 1.0;
    if (canvas.fit == 1u) { scale = contain_scale; }
    if (canvas.fit == 2u) { scale = cover_scale; }
    let shown = canvas.image_size * scale;
    let mapped = (uv * target_size - (target_size - shown) * 0.5) / shown;
    let inside = select(0.0, 1.0, all(mapped >= vec2f(0.0)) && all(mapped <= vec2f(1.0)));
    return vec3f(mapped, select(inside, 1.0, canvas.fit == 2u));
}

fn linear_to_srgb(value: vec3f) -> vec3f {
    let low = value * 12.92;
    let high = 1.055 * pow(max(value, vec3f(0.0)), vec3f(1.0 / 2.4)) - 0.055;
    return select(high, low, value <= vec3f(0.0031308));
}

@fragment
fn canvas_fs(input: CanvasVaryings) -> @location(0) vec4f {
    var coverage = 1.0;
    var texel = vec4f(1.0);
    let size = max(canvas.rect.zw, vec2f(1.0));
    if (canvas.kind == 1u) {
        coverage = 1.0 - smoothstep(-0.75, 0.75, rounded_distance(input.uv, size, input.radii));
    } else if (canvas.kind == 2u) {
        let distance = rounded_distance(input.uv, size, input.radii);
        let inner = max(input.extra.x, 0.5);
        coverage = (1.0 - smoothstep(-0.75, 0.75, distance)) * smoothstep(-inner - 0.75, -inner + 0.75, distance);
    } else if (canvas.kind == 3u) {
        let distance = rounded_distance(input.uv, size, input.radii);
        let blur = max(input.extra.x, 0.75);
        coverage = 1.0 - smoothstep(-blur, blur, distance);
    } else if (canvas.kind == 4u) {
        let mapped = image_uv(input.uv);
        texel = textureSampleLevel(canvas_image, canvas_sampler, mapped.xy, 0.0) * mapped.z;
    } else if (canvas.kind == 5u) {
        coverage = textureSampleLevel(canvas_image, canvas_sampler, input.uv, 0.0).r;
    }
    let alpha = clamp(input.color.a * texel.a * coverage, 0.0, 1.0);
    let straight_rgb = max(input.color.rgb * texel.rgb, vec3f(0.0));
    var rgb = straight_rgb * alpha;
    if (canvas.target_srgb != 0u) { rgb = linear_to_srgb(straight_rgb) * alpha; }
    return vec4f(rgb, alpha);
}
