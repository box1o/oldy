#pragma once

#include <map>

#include <woki/rhi/render_pass_encoder.hpp>

#include "../handles.hpp"
#include "draw_packet.hpp"
#include "material.hpp"
#include "material_library.hpp"
#include "pipeline_cache.hpp"
#include "render_graph.hpp"
#include "../scene.hpp"

namespace woki::gfx {

enum class GpuDrivenFallbackReason : u8 {
    None,
    Disabled,
    MissingCompute,
    MissingIndirect,
    MissingIndirectCount,
    MissingStorage,
    ProgramsUnavailable,
    SceneUnavailable,
    MeshStreamsUnavailable,
    PipelinePending,
    CapacityOverflow,
    ValidationFailure,
};

struct IndirectDrawLayout final {
    u32 command_stride{sizeof(rhi::DrawIndexedIndirectArguments)};
    u32 count_stride{sizeof(rhi::IndirectDrawCount)};
    u32 visible_instance_stride{sizeof(u32) * 2U};
    u32 visible_meshlet_stride{sizeof(u32)};
};

struct IndirectBinKey final {
    GraphicsPipelineKey pipeline;
    MaterialGpuHandle material;
    MeshResidentHandle mesh;
    u32 lod{};
    [[nodiscard]] friend auto operator<=>(const IndirectBinKey&, const IndirectBinKey&) = default;
};

struct IndirectBin final {
    IndirectBinKey key;
    RenderPhase phase{RenderPhase::Opaque};
    u32 first_command{};
    u32 command_capacity{};
    u32 count_index{};
    u32 first_candidate{};
    u32 candidate_count{};
};

struct IndirectDrawStream final {
    GraphBuffer commands;
    GraphBufferRef commands_version;
    GraphBuffer counts;
    GraphBufferRef counts_version;
    GraphBuffer visible_instances;
    GraphBufferRef visible_instances_version;
    GraphBuffer visible_meshlets;
    GraphBufferRef visible_meshlets_version;
    GraphBuffer diagnostics;
    GraphBufferRef diagnostics_version;
    IndirectDrawLayout layout;
    std::vector<IndirectBin> bins;
    u32 command_capacity{};
    u32 meshlet_capacity{};
    bool ready{};
};

struct GpuVisibilityStats final {
    u64 candidates{};
    u64 layer_culled{};
    u64 frustum_culled{};
    u64 occlusion_culled{};
    u64 visible_objects{};
    u64 lod_changes{};
    u64 meshlets_tested{};
    u64 meshlets_cone_culled{};
    u64 meshlets_bounds_culled{};
    u64 visible_meshlets{};
    u64 bins{};
    u64 indirect_commands{};
    u64 command_overflow{};
    u64 meshlet_overflow{};
    GpuDrivenFallbackReason fallback{GpuDrivenFallbackReason::Disabled};
};

struct GpuVisibilityDiagnostics final {
    std::array<u32, 13> counters{};
};

struct alignas(16) GpuVisibilityCandidate final {
    std::array<f32, 4> sphere{};
    std::array<f32, 4> bounds_extent_lod_scale{};
    std::array<u32, 4> masks{};        // object low/high, view low/high
    std::array<u32, 4> lod_meshlets{}; // LOD offset/count, meshlet offset/count
    std::array<u32, 4> bin{};          // first command, capacity, count index, object index
    std::array<u32, 4> flags{};        // render phase and reserved policy bits
};

struct alignas(16) GpuLodRecord final {
    f32 geometric_error{};
    u32 index_count{};
    u32 first_index{};
    i32 base_vertex{};
    u32 lod{};
    std::array<u32, 3> padding{};
};

struct alignas(16) GpuVisibilityParams final {
    std::array<std::array<f32, 4>, 6> frustum_planes{};
    std::array<std::array<f32, 4>, 4> view_projection{};
    std::array<f32, 4> camera_position_threshold{};
    std::array<u32, 4> dimensions_flags{}; // width, height, mip count, flags
    std::array<u32, 4> counts{};           // candidates, command capacity, meshlets, meshlet capacity
};

static_assert(sizeof(GpuVisibilityCandidate) == 96);
static_assert(sizeof(GpuLodRecord) == 32);
static_assert(sizeof(GpuVisibilityParams) == 208);
static_assert(sizeof(GpuVisibilityDiagnostics) == 52);

struct PsoWarmupEntry final {
    GraphicsPipelineKey pipeline;
    MaterialGpuHandle material;
    [[nodiscard]] friend auto operator<=>(const PsoWarmupEntry&, const PsoWarmupEntry&) = default;
};

struct PsoWarmupManifest final {
    std::vector<PsoWarmupEntry> reachable;
};

struct GpuVisibilityOutput final {
    IndirectDrawStream stream;
    PsoWarmupManifest warmup;
    bool hiz_used{};
    bool validation_pending{};
    GpuDrivenFallbackReason fallback{GpuDrivenFallbackReason::Disabled};
};

struct HiZDepthPyramidOutput final {
    GraphTexture previous;
    GraphTextureRef previous_version;
    GraphTexture current;
    GraphTextureRef current_version;
    u32 mip_count{};
    bool history_valid{};
};

// Physical table slots are short-lived shader references, never asset or scene
// identities. Rebuild invalidates every old index after device loss.
class GpuResourceIndex final {
public:
    [[nodiscard]] u32 Insert(u64 physical_key);
    [[nodiscard]] std::optional<u32> Resolve(u64 physical_key) const noexcept;
    void Rebuild() noexcept;

    [[nodiscard]] u64 Generation() const noexcept {
        return generation_;
    }

private:
    std::map<u64, u32> indices_;
    u64 generation_{1};
};

[[nodiscard]] PsoWarmupManifest BuildPsoWarmupManifest(std::span<const PsoWarmupEntry> reachable);
[[nodiscard]] std::string_view GpuDrivenFallbackName(GpuDrivenFallbackReason reason) noexcept;

} // namespace woki::gfx
