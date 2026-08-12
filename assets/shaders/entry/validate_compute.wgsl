struct ValidationData { dimensions: vec3u, element_count: u32 };
@group(3) @binding(0) var<uniform> validation: ValidationData;
@group(3) @binding(1) var<storage, read_write> validation_output: array<u32>;
@compute @workgroup_size(8, 8, 1) fn validate_compute(@builtin(global_invocation_id) global_id: vec3u) {
    if (!dispatch_in_bounds(global_id, validation.dimensions)) { return; }
    let index = global_linear_index(global_id, validation.dimensions);
    if (index < validation.element_count) { validation_output[index] = index; }
}
