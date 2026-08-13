#pragma once

#include <string>
#include <vector>

#include <woki/core.hpp>

namespace woki::gfx {

struct Capabilities final {
    bool compute{};
    bool timestamp_queries{};
    bool texture_compression{};
    u32 maximum_texture_dimension_2d{};
    u64 maximum_buffer_size{};
};

struct GraphStats final {
    u64 deterministic_hash{};
    u64 transient_bytes{};
    u32 pass_count{};
};

struct ResourceStats final {
    u64 resident_bytes{};
    u64 pending_bytes{};
    u64 deferred_release_count{};
};

struct RuntimeStats final {
    u64 frame_number{};
    u64 submitted_frames{};
    u64 rejected_compute_jobs{};
    u64 visible_objects{};
    u64 draw_packets{};
    u64 draw_calls{};
    u64 skipped_resources{};
    u64 fallback_textures{};
    ResourceStats resources;
};

struct BackendDiagnostic final {
    std::string backend_name;
    std::string adapter_name;
};

class Diagnostics final {
public:
    Diagnostics() = default;

    [[nodiscard]] const RuntimeStats& Runtime() const noexcept {
        return runtime_;
    }

    [[nodiscard]] const BackendDiagnostic& Backend() const noexcept {
        return backend_;
    }

private:
    friend class RenderRuntime;
    RuntimeStats runtime_;
    BackendDiagnostic backend_;
};

} // namespace woki::gfx
