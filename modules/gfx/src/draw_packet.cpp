#include <algorithm>

#include <woki/gfx/advanced/draw_packet.hpp>

namespace woki::gfx {

Result<std::vector<DrawPacket>> DrawPacketBuilder::Build(const std::span<const DrawPacketMesh> visible) const {
    std::vector<DrawPacket> packets;
    for (const auto& item : visible) {
        if (item.product == nullptr || item.lod >= item.product->lods.size() || !item.mesh.IsValid())
            return Err(ErrorCode::ValidationOutOfRange, "draw packet mesh or LOD is invalid");
        const auto& lod = item.product->lods[item.lod];
        for (u32 submesh_index = 0; submesh_index < lod.submeshes.size(); ++submesh_index) {
            const auto& submesh = lod.submeshes[submesh_index];
            if (submesh.material_slot >= item.materials.size())
                return Err(ErrorCode::ValidationOutOfRange, "draw packet material slot is unresolved");
            auto pipeline = item.pipeline;
            pipeline.vertex_schema_id = item.product->schema.id;
            const auto material = item.materials[submesh.material_slot];
            if (submesh.skin != std::numeric_limits<u32>::max() && submesh.skin >= item.skin_palettes.size())
                return Err(ErrorCode::ValidationOutOfRange, "draw packet skin palette is unresolved");
            const auto palette = submesh.skin != std::numeric_limits<u32>::max() && submesh.skin < item.skin_palettes.size() ? item.skin_palettes[submesh.skin] : item.palette;
            const u64 sort_key = (static_cast<u64>(material.Index() & 0xffffU) << 48U) | (static_cast<u64>(item.mesh.Index() & 0xffffffU) << 24U) | submesh_index;
            packets.push_back({std::move(pipeline), material, item.mesh, palette, submesh.first_index, submesh.index_count, submesh.vertex_offset, item.instance_offset, sort_key});
        }
    }
    std::sort(packets.begin(), packets.end(), [](const auto& left, const auto& right) {
        if (left.pipeline != right.pipeline)
            return left.pipeline < right.pipeline;
        if (left.material != right.material)
            return left.material < right.material;
        if (left.mesh != right.mesh)
            return left.mesh < right.mesh;
        if (left.first_index != right.first_index)
            return left.first_index < right.first_index;
        return left.sort_key < right.sort_key;
    });
    return Ok(std::move(packets));
}

} // namespace woki::gfx
