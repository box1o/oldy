struct ExposureParams {
    dimensions: vec2u,
    delta_time: f32,
    key: f32,
    limits: vec2f,
    speeds: vec2f,
};

struct ExposureValue {
    value: f32,
    valid: u32,
    _padding: vec2u,
};

@group(0) @binding(0) var<uniform> exposure_params: ExposureParams;
@group(0) @binding(1) var exposure_source: texture_2d<f32>;
@group(0) @binding(2) var<storage, read_write> luminance_histogram: array<atomic<u32>, 256>;
@group(0) @binding(3) var<storage, read_write> adapted_exposure: ExposureValue;

@compute @workgroup_size(8, 8)
fn luminance_histogram_cs(@builtin(global_invocation_id) id: vec3u) {
    if (any(id.xy >= exposure_params.dimensions)) {
        return;
    }
    let color = textureLoad(exposure_source, vec2i(id.xy), 0).rgb;
    let luminance = max(dot(color, vec3f(0.2126, 0.7152, 0.0722)), 0.000001);
    let bin = u32(clamp((log2(luminance) + 12.0) * (255.0 / 24.0), 0.0, 255.0));
    atomicAdd(&luminance_histogram[bin], 1u);
}

var<workgroup> weighted: array<f32, 256>;
var<workgroup> counts: array<u32, 256>;

@compute @workgroup_size(256)
fn exposure_reduce_cs(@builtin(local_invocation_index) index: u32) {
    let count = atomicLoad(&luminance_histogram[index]);
    counts[index] = count;
    weighted[index] = f32(count) * (f32(index) * (24.0 / 255.0) - 12.0);
    workgroupBarrier();
    var stride = 128u;
    loop {
        if (index < stride) {
            counts[index] += counts[index + stride];
            weighted[index] += weighted[index + stride];
        }
        workgroupBarrier();
        if (stride == 1u) { break; }
        stride /= 2u;
    }
    if (index == 0u) {
        let average = weighted[0] / max(f32(counts[0]), 1.0);
        let target_exposure = clamp(exposure_params.key / exp2(average), exposure_params.limits.x, exposure_params.limits.y);
        let previous = select(target_exposure, adapted_exposure.value, adapted_exposure.valid != 0u);
        let speed = select(exposure_params.speeds.y, exposure_params.speeds.x, target_exposure > previous);
        adapted_exposure.value = mix(previous, target_exposure, 1.0 - exp(-speed * max(exposure_params.delta_time, 0.0)));
        adapted_exposure.valid = 1u;
    }
}
