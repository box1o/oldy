#pragma once

#include <woki/core.hpp>

#include "draw_packet.hpp"
#include "mesh_product.hpp"
#include "runtime_services.hpp"
#include "../handles.hpp"

namespace woki::gfx {

struct DynamicMeshTag;
using DynamicMeshHandle = Handle<DynamicMeshTag>;

struct MeshResident final {
    std::vector<BufferSlice> vertices;
    std::vector<BufferSlice> indices;
    std::vector<BufferSlice> meshlets;
    std::vector<BufferSlice> meshlet_descriptors;
    std::vector<BufferSlice> meshlet_bounds;
    std::vector<BufferSlice> meshlet_vertices;
    std::vector<BufferSlice> meshlet_triangles;
    std::vector<u32> meshlet_counts;
    u32 first_resident_lod{};
    u32 last_resident_lod{};
    rhi::SubmissionTicket last_used;
    u64 version{};
};

struct MeshLodSelection final {
    u32 requested{};
    u32 resident{};
};

[[nodiscard]] MeshLodSelection SelectMeshLod(std::span<const MeshLod> lods, f32 projected_scale, f32 threshold, u32 previous, u32 first_resident, u32 last_resident, f32 hysteresis = 0.15F) noexcept;

class MeshLibrary final {
public:
    MeshLibrary(asset::AssetManager& assets, RenderRuntimeServices& runtime);
    ~MeshLibrary();
    [[nodiscard]] Result<MeshHandle> Request(asset::AssetId id);
    [[nodiscard]] Result<void> Pump();
    [[nodiscard]] std::vector<asset::AssetId> DrainGeneratedMaterials();
    [[nodiscard]] MeshState State(MeshHandle handle) const noexcept;
    [[nodiscard]] const MeshProduct* Product(MeshHandle handle) const noexcept;
    [[nodiscard]] const MeshResident* Resident(MeshHandle handle) const noexcept;
    [[nodiscard]] const MeshResident* ResidentFromPacket(MeshResidentHandle handle) const noexcept;
    [[nodiscard]] MeshResidentHandle PacketHandle(MeshHandle handle) const noexcept;
    [[nodiscard]] Result<void> Evict(MeshHandle handle);
    void MarkUsed(MeshHandle handle, rhi::SubmissionTicket submission);
    void MarkDeviceLost() noexcept;
    [[nodiscard]] Result<scope<MeshLibrary>> PrepareReplacement(RenderRuntimeServices& runtime) const;

private:
    struct Impl;
    scope<Impl> impl_;
};

struct DynamicMeshDesc final {
    VertexSchema schema;
    u64 vertex_capacity{};
    u64 index_capacity{};
};

struct DynamicMeshRecord final {
    DynamicMeshDesc descriptor;
    BufferSlice vertices;
    BufferSlice indices;
    u64 version{};
    rhi::SubmissionTicket last_used;
};

class DynamicMeshLibrary final {
public:
    DynamicMeshLibrary(BufferPool& pool, UploadScheduler& uploads);
    ~DynamicMeshLibrary();
    [[nodiscard]] Result<DynamicMeshHandle> Create(DynamicMeshDesc descriptor);
    [[nodiscard]] Result<u64> Update(DynamicMeshHandle handle, std::span<const std::byte> vertices, std::span<const std::byte> indices);
    [[nodiscard]] const DynamicMeshRecord* TryGet(DynamicMeshHandle handle) const noexcept;
    [[nodiscard]] Result<void> Destroy(DynamicMeshHandle handle);
    void MarkUsed(DynamicMeshHandle handle, rhi::SubmissionTicket submission) noexcept;

private:
    struct Slot final {
        DynamicMeshRecord record;
    };

    BufferPool& pool_;
    UploadScheduler& uploads_;
    SlotMap<Slot, DynamicMeshHandle> slots_;
    std::vector<DynamicMeshHandle> active_;
};

} // namespace woki::gfx
