fn median3(value: vec3f) -> f32 { return max(min(value.r, value.g), min(max(value.r, value.g), value.b)); }
fn screen_space_coverage(signed_distance: f32, width: f32) -> f32 { return saturate(signed_distance / max(width, 0.000001) + 0.5); }
fn bitmap_coverage(sample_value: f32, cutoff: f32) -> f32 { return select(0.0, sample_value, sample_value >= cutoff); }
fn sdf_coverage(sample_value: f32, smoothing: f32) -> f32 { return screen_space_coverage(sample_value - 0.5, smoothing); }
fn msdf_coverage(sample_value: vec3f, smoothing: f32) -> f32 { return screen_space_coverage(median3(sample_value) - 0.5, smoothing); }
