#pragma once

#include "animation.hpp"
#include "material_library.hpp"
#include "pipeline_cache.hpp"
#include "mesh_product.hpp"

namespace woki::gfx {

struct MeshResidentTag;
using MeshResidentHandle = Handle<MeshResidentTag>;

struct DrawPacket final {
    GraphicsPipelineKey pipeline;
    MaterialGpuHandle material;
    MeshResidentHandle mesh;
    SkinPaletteHandle palette;
    u32 first_index{};
    u32 index_count{};
    i32 vertex_offset{};
    u32 instance_offset{};
    u64 sort_key{};
};

struct DrawPacketMesh final {
    MeshResidentHandle mesh;
    const MeshProduct* product{};
    u32 lod{};
    std::span<const MaterialGpuHandle> materials;
    GraphicsPipelineKey pipeline;
    SkinPaletteHandle palette;
    std::span<const SkinPaletteHandle> skin_palettes;
    u32 instance_offset{};
};

class DrawPacketBuilder final {
public:
    [[nodiscard]] Result<std::vector<DrawPacket>> Build(std::span<const DrawPacketMesh> visible) const;
};

} // namespace woki::gfx
