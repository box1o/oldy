#pragma once

#include <array>
#include <limits>

#include <woki/math.hpp>

#include "mesh.hpp"

namespace woki::gfx {

inline constexpr u32 kMeshProductVersion = 5;

struct MeshChunk final {
    u64 offset{};
    u64 size{};
    ContentHash checksum;
};

struct MeshSubmesh final {
    u32 material_slot{};
    u32 first_index{};
    u32 index_count{};
    i32 vertex_offset{};
    u32 skin{std::numeric_limits<u32>::max()};
};

struct MeshLod final {
    f32 ratio{1.0F};
    f32 geometric_error{};
    u32 vertex_count{};
    u32 index_count{};
    MeshChunk vertices;
    MeshChunk indices;
    MeshChunk meshlets;
    std::vector<MeshSubmesh> submeshes;
};

struct MeshJoint final {
    std::string name;
    i32 parent{-1};
    math::mat4f bind_local{math::mat4f::identity()};
    math::mat4f inverse_bind{math::mat4f::identity()};
};

struct MeshAnimationChannel final {
    u32 joint{};
    AnimationPath path{AnimationPath::Translation};
    AnimationInterpolation interpolation{AnimationInterpolation::Linear};
    std::vector<f32> times;
    std::vector<math::vec4f> values;
};

struct MeshAnimationClip final {
    std::string name;
    u32 skeleton{};
    f32 duration{};
    std::vector<MeshAnimationChannel> channels;
};

struct MeshImage final {
    std::string uri;
    std::string mime_type;
    bool embedded{};
    MeshChunk payload;
};

struct MeshProduct final {
    VertexSchema schema;
    math::vec3f bounds_min{};
    math::vec3f bounds_max{};
    std::vector<MeshLod> lods; // coarse first
    std::vector<MeshJoint> skeleton;
    std::vector<MeshAnimationClip> animations;
    u32 mesh_count{};
    std::vector<ImportedNode> nodes;
    std::vector<ImportedSkin> skins;
    std::vector<ImportedSkeleton> source_skeletons;
    std::vector<ImportedMaterial> materials;
    std::vector<asset::AssetId> generated_material_instances;
    std::vector<MeshImage> images;
    std::vector<ImportedDiagnostic> diagnostics;
    std::vector<asset::ProductDependency> dependencies;
    SourceConversion conversion;
    std::string importer;
    std::span<const std::byte> bytes;

    [[nodiscard]] std::span<const std::byte> Chunk(const MeshChunk& chunk) const noexcept;
};

struct MeshProductLimits final {
    u64 max_bytes{1024ULL * 1024ULL * 1024ULL};
    u32 max_lods{16};
    u32 max_submeshes{65'536};
    u32 max_joints{65'535};
    u32 max_clips{4096};
    u32 max_keys{16'777'216};
};

struct MeshLodPayload final {
    std::vector<std::byte> vertices;
    std::vector<std::byte> indices;
    std::vector<std::byte> meshlets;
};

struct MeshletDescriptor final {
    u32 submesh{};
    u32 vertex_offset{};
    u32 vertex_count{};
    u32 triangle_offset{};
    u32 triangle_count{};
};

struct MeshletBounds final {
    std::array<f32, 4> sphere{};
    std::array<f32, 4> cone{};
};

struct MeshletStreams final {
    std::vector<MeshletDescriptor> descriptors;
    std::vector<MeshletBounds> bounds;
    std::vector<u32> vertices;
    std::vector<u8> triangles;
};

// Decodes the independently addressable streams from the cooked meshlet chunk.
// The classic indexed LOD remains authoritative when this optional data is absent.
[[nodiscard]] Result<MeshletStreams> DecodeMeshletStreams(std::span<const std::byte> bytes);

[[nodiscard]] Result<MeshProduct> ParseMeshProduct(std::span<const std::byte> bytes, MeshProductLimits limits = {});
[[nodiscard]] Result<MeshProduct> ParseMeshProductHeader(std::span<const std::byte> bytes, MeshProductLimits limits = {});
[[nodiscard]] Result<MeshProduct> ReadMeshProductHeader(const asset::ProductReader& reader, MeshProductLimits limits = {});
[[nodiscard]] Result<MeshLodPayload> ReadMeshLod(const asset::ProductReader& reader, u32 lod);

#ifndef __EMSCRIPTEN__
[[nodiscard]] Result<std::vector<std::byte>> BuildMeshProduct(const ImportedScene& scene,
    const MeshSource& source,
    std::string_view importer,
    std::span<const asset::ProductDependency> dependencies = {},
    MeshProductLimits limits = {});
#endif

} // namespace woki::gfx
