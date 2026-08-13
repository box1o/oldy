#include <unordered_map>
#include <algorithm>
#include <cmath>

#include <woki/gfx/advanced/mesh_library.hpp>

namespace woki::gfx {

MeshLodSelection SelectMeshLod(const std::span<const MeshLod> lods, const f32 projected_scale, const f32 threshold, const u32 previous, const u32 first_resident, const u32 last_resident, const f32 hysteresis) noexcept {
    if (lods.empty())
        return {};
    u32 requested = static_cast<u32>(lods.size() - 1);
    for (u32 index = 0; index < lods.size(); ++index)
        if (lods[index].geometric_error * projected_scale <= threshold) {
            requested = index;
            break;
        }
    if (previous < lods.size() && requested != previous) {
        const f32 boundary = lods[std::min(requested, previous)].geometric_error * projected_scale;
        if (std::abs(boundary - threshold) <= threshold * hysteresis)
            requested = previous;
    }
    return {requested, std::clamp(requested, first_resident, last_resident)};
}

struct MeshLibrary::Impl final {
    struct Slot {
        asset::AssetId id;
        MeshState state{MeshState::Unloaded};
        task::Future<asset::AssetLease> pending;
        asset::AssetLease lease;
        std::optional<MeshProduct> product;
        MeshResident resident;
        std::shared_ptr<UploadPublicationToken> publication;
        std::string diagnostic;
    };

    asset::AssetManager& assets;
    RenderRuntimeServices* runtime;
    SlotMap<Slot, MeshHandle> slots;
    std::unordered_map<asset::AssetId, MeshHandle> handles;
    std::vector<asset::AssetId> generated_materials;

    Impl(asset::AssetManager& assets_value, RenderRuntimeServices& runtime_value)
        : assets(assets_value),
          runtime(&runtime_value) {}
};

MeshLibrary::MeshLibrary(asset::AssetManager& assets, RenderRuntimeServices& runtime)
    : impl_(createScope<Impl>(assets, runtime)) {}

MeshLibrary::~MeshLibrary() {
    impl_->slots.ForEach([this](const MeshHandle handle, const Impl::Slot& slot) {
        if (!slot.resident.vertices.empty())
            static_cast<void>(Evict(handle));
    });
}

Result<MeshHandle> MeshLibrary::Request(const asset::AssetId id) {
    if (const auto found = impl_->handles.find(id); found != impl_->handles.end()) {
        auto& slot = impl_->slots.Get(found->second);
        if ((slot.state == MeshState::Evicted || slot.state == MeshState::Lost) && slot.product)
            slot.state = MeshState::CpuReady;
        return Ok(found->second);
    }
    auto future = impl_->assets.Request(id);
    if (!future)
        return Err(std::move(future).error());
    Impl::Slot slot;
    slot.id = id;
    slot.state = MeshState::Loading;
    slot.pending = std::move(*future);
    const MeshHandle handle = impl_->slots.Emplace(std::move(slot));
    impl_->handles.emplace(id, handle);
    return Ok(handle);
}

Result<void> MeshLibrary::Pump() {
    BufferPool* vertex_pool = impl_->runtime->Pool(BufferPoolClass::Vertex);
    BufferPool* index_pool = impl_->runtime->Pool(BufferPoolClass::Index);
    for (const auto handle : impl_->slots.Handles()) {
        auto& slot = impl_->slots.Get(handle);
        if (slot.state == MeshState::Loading && slot.pending.IsReady()) {
            auto loaded = slot.pending.Wait();
            if (!loaded) {
                slot.state = MeshState::Failed;
                slot.diagnostic = loaded.error().Message();
                continue;
            }
            slot.lease = *loaded;
            auto parsed = ParseMeshProduct(slot.lease.Bytes());
            if (!parsed) {
                slot.state = MeshState::Failed;
                slot.diagnostic = parsed.error().Message();
                continue;
            }
            slot.product = std::move(*parsed);
            impl_->generated_materials.insert(impl_->generated_materials.end(), slot.product->generated_material_instances.begin(), slot.product->generated_material_instances.end());
            slot.state = MeshState::CpuReady;
        }
        if (slot.state == MeshState::CpuReady && vertex_pool != nullptr && index_pool != nullptr && slot.product) {
            const u32 lod_index = slot.resident.vertices.empty() ? 0U : slot.resident.last_resident_lod + 1U;
            if (lod_index >= slot.product->lods.size()) {
                slot.state = MeshState::Resident;
                continue;
            }
            const auto& lod = slot.product->lods[lod_index];
            BufferSlice vertices, indices, meshlets;
            TRY_ASSIGN(vertices, vertex_pool->Allocate(lod.vertices.size, 16));
            auto index_result = index_pool->Allocate(lod.indices.size, 4);
            if (!index_result) {
                static_cast<void>(vertex_pool->Free(vertices.allocation));
                return Err(std::move(index_result).error());
            }
            indices = *index_result;
            BufferPool* meshlet_pool = impl_->runtime->Pool(BufferPoolClass::Indirect);
            MeshletStreams meshlet_streams;
            std::array<BufferSlice, 4> separated{};
            if (meshlet_pool != nullptr && lod.meshlets.size != 0) {
                auto decoded = DecodeMeshletStreams(slot.product->Chunk(lod.meshlets));
                if (!decoded) {
                    static_cast<void>(vertex_pool->Free(vertices.allocation));
                    static_cast<void>(index_pool->Free(indices.allocation));
                    return Err(std::move(decoded).error());
                }
                meshlet_streams = std::move(*decoded);
                auto meshlet_result = meshlet_pool->Allocate(lod.meshlets.size, 4);
                if (!meshlet_result) {
                    static_cast<void>(vertex_pool->Free(vertices.allocation));
                    static_cast<void>(index_pool->Free(indices.allocation));
                    return Err(std::move(meshlet_result).error());
                }
                meshlets = *meshlet_result;
                const std::array<u64, 4> sizes{meshlet_streams.descriptors.size() * sizeof(MeshletDescriptor), meshlet_streams.bounds.size() * sizeof(MeshletBounds), meshlet_streams.vertices.size() * sizeof(u32),
                    (meshlet_streams.triangles.size() + 3U) & ~u64{3}};
                for (u32 stream = 0; stream < sizes.size(); ++stream) {
                    if (sizes[stream] == 0)
                        continue;
                    auto allocation = meshlet_pool->Allocate(sizes[stream], stream == 3 ? 4 : 16);
                    if (!allocation) {
                        static_cast<void>(vertex_pool->Free(vertices.allocation));
                        static_cast<void>(index_pool->Free(indices.allocation));
                        static_cast<void>(meshlet_pool->Free(meshlets.allocation));
                        for (const auto& existing : separated)
                            if (existing.allocation.IsValid())
                                static_cast<void>(meshlet_pool->Free(existing.allocation));
                        return Err(std::move(allocation).error());
                    }
                    separated[stream] = *allocation;
                }
            }
            ResidencyRecord publication;
            publication.resource = ResourceState::Ready;
            publication.residency = ResidencyState::UploadPending;
            publication.content_version = ContentVersion(slot.lease.Get().version.generation);
            publication.residency_version = ResidencyVersion(slot.resident.version + 1);
            const u32 upload_count = 2U + (meshlets.allocation.IsValid() ? 1U : 0U) + static_cast<u32>(std::ranges::count_if(separated, [](const BufferSlice& slice) { return slice.allocation.IsValid(); }));
            slot.publication = UploadPublicationToken::Create(publication, upload_count);
            std::vector<BufferUploadRequest> uploads;
            uploads.push_back({vertex_pool->SharedBuffer(), vertices.offset, std::vector<std::byte>(slot.product->Chunk(lod.vertices).begin(), slot.product->Chunk(lod.vertices).end()), slot.publication});
            uploads.push_back({index_pool->SharedBuffer(), indices.offset, std::vector<std::byte>(slot.product->Chunk(lod.indices).begin(), slot.product->Chunk(lod.indices).end()), slot.publication});
            if (meshlets.allocation.IsValid())
                uploads.push_back({meshlet_pool->SharedBuffer(), meshlets.offset, std::vector<std::byte>(slot.product->Chunk(lod.meshlets).begin(), slot.product->Chunk(lod.meshlets).end()), slot.publication});
            const auto append_stream = [&](const BufferSlice& target, const void* data, const size_t size) {
                if (!target.allocation.IsValid())
                    return;
                const auto* first = static_cast<const std::byte*>(data);
                uploads.push_back({meshlet_pool->SharedBuffer(), target.offset, std::vector<std::byte>(first, first + size), slot.publication});
            };
            append_stream(separated[0], meshlet_streams.descriptors.data(), meshlet_streams.descriptors.size() * sizeof(MeshletDescriptor));
            append_stream(separated[1], meshlet_streams.bounds.data(), meshlet_streams.bounds.size() * sizeof(MeshletBounds));
            append_stream(separated[2], meshlet_streams.vertices.data(), meshlet_streams.vertices.size() * sizeof(u32));
            append_stream(separated[3], meshlet_streams.triangles.data(), meshlet_streams.triangles.size());
            auto queued = impl_->runtime->Uploads().EnqueueBatch(std::move(uploads));
            if (!queued) {
                slot.publication->Fail(std::string(queued.error().Message()));
                static_cast<void>(vertex_pool->Free(vertices.allocation));
                static_cast<void>(index_pool->Free(indices.allocation));
                if (meshlets.allocation.IsValid())
                    static_cast<void>(meshlet_pool->Free(meshlets.allocation));
                for (const auto& allocation : separated)
                    if (allocation.allocation.IsValid())
                        static_cast<void>(meshlet_pool->Free(allocation.allocation));
                slot.state = MeshState::Failed;
                slot.diagnostic = std::string(queued.error().Message());
                continue;
            }
            slot.resident.vertices.push_back(vertices);
            slot.resident.indices.push_back(indices);
            if (meshlets.allocation.IsValid())
                slot.resident.meshlets.push_back(meshlets);
            slot.resident.meshlet_descriptors.push_back(separated[0]);
            slot.resident.meshlet_bounds.push_back(separated[1]);
            slot.resident.meshlet_vertices.push_back(separated[2]);
            slot.resident.meshlet_triangles.push_back(separated[3]);
            slot.resident.meshlet_counts.push_back(static_cast<u32>(meshlet_streams.descriptors.size()));
            slot.resident.first_resident_lod = 0;
            slot.resident.last_resident_lod = lod_index;
            ++slot.resident.version;
            slot.state = MeshState::Uploading;
        }
        if (slot.state == MeshState::Uploading && slot.publication) {
            const auto publication = slot.publication->Snapshot();
            if (publication.resource == ResourceState::Failed) {
                slot.state = MeshState::Failed;
                slot.diagnostic = "mesh upload failed";
            } else if (publication.residency == ResidencyState::Resident) {
                slot.state = slot.product && slot.resident.last_resident_lod + 1U < slot.product->lods.size() ? MeshState::CpuReady : MeshState::Resident;
            }
        }
    }
    return Ok();
}

std::vector<asset::AssetId> MeshLibrary::DrainGeneratedMaterials() {
    auto result = std::move(impl_->generated_materials);
    impl_->generated_materials.clear();
    std::ranges::sort(result);
    result.erase(std::unique(result.begin(), result.end()), result.end());
    return result;
}

MeshState MeshLibrary::State(const MeshHandle handle) const noexcept {
    const auto* slot = impl_->slots.TryGet(handle);
    return slot == nullptr ? MeshState::Failed : slot->state;
}

const MeshProduct* MeshLibrary::Product(const MeshHandle handle) const noexcept {
    const auto* slot = impl_->slots.TryGet(handle);
    return slot == nullptr || State(handle) == MeshState::Failed || !slot->product ? nullptr : &*slot->product;
}

const MeshResident* MeshLibrary::Resident(const MeshHandle handle) const noexcept {
    const auto state = State(handle);
    const auto* slot = impl_->slots.TryGet(handle);
    return state == MeshState::Resident && slot != nullptr ? &slot->resident : nullptr;
}

const MeshResident* MeshLibrary::ResidentFromPacket(const MeshResidentHandle handle) const noexcept {
    return Resident(MeshHandle::Create(handle.Index(), handle.Generation()));
}

MeshResidentHandle MeshLibrary::PacketHandle(const MeshHandle handle) const noexcept {
    return Resident(handle) == nullptr ? MeshResidentHandle{} : MeshResidentHandle::Create(handle.Index(), handle.Generation());
}

Result<void> MeshLibrary::Evict(const MeshHandle handle) {
    if (!impl_->slots.Contains(handle))
        return Err(ErrorCode::InvalidState, "mesh handle is invalid");
    auto& slot = impl_->slots.Get(handle);
    if (auto* pool = impl_->runtime->Pool(BufferPoolClass::Vertex))
        for (const auto allocation : slot.resident.vertices)
            TRY_VOID(pool->Free(allocation.allocation, slot.resident.last_used));
    if (auto* pool = impl_->runtime->Pool(BufferPoolClass::Index))
        for (const auto allocation : slot.resident.indices)
            TRY_VOID(pool->Free(allocation.allocation, slot.resident.last_used));
    if (auto* pool = impl_->runtime->Pool(BufferPoolClass::Indirect))
        for (const auto allocation : slot.resident.meshlets)
            TRY_VOID(pool->Free(allocation.allocation, slot.resident.last_used));
    if (auto* pool = impl_->runtime->Pool(BufferPoolClass::Indirect)) {
        for (const auto* stream : {&slot.resident.meshlet_descriptors, &slot.resident.meshlet_bounds, &slot.resident.meshlet_vertices, &slot.resident.meshlet_triangles})
            for (const auto allocation : *stream)
                TRY_VOID(pool->Free(allocation.allocation, slot.resident.last_used));
    }
    slot.resident = {};
    slot.state = MeshState::Evicted;
    return Ok();
}

void MeshLibrary::MarkUsed(const MeshHandle handle, const rhi::SubmissionTicket submission) {
    if (auto* slot = impl_->slots.TryGet(handle))
        slot->resident.last_used = submission;
}

void MeshLibrary::MarkDeviceLost() noexcept {
    impl_->slots.ForEach([](const MeshHandle, Impl::Slot& slot) {
        if (slot.state == MeshState::Resident || slot.state == MeshState::Uploading) {
            slot.state = MeshState::Lost;
            slot.resident = {};
            if (slot.publication)
                slot.publication->MarkDeviceLost();
        }
    });
}

Result<scope<MeshLibrary>> MeshLibrary::PrepareReplacement(RenderRuntimeServices& runtime) const {
    auto replacement = createScope<MeshLibrary>(impl_->assets, runtime);
    for (const auto handle : impl_->slots.Handles()) {
        MeshHandle cloned;
        TRY_ASSIGN(cloned, replacement->Request(impl_->slots.Get(handle).id));
        if (cloned != handle)
            return Err(ErrorCode::InvalidState, "mesh replacement changed a logical handle");
    }
    return Ok(std::move(replacement));
}

DynamicMeshLibrary::DynamicMeshLibrary(BufferPool& pool, UploadScheduler& uploads)
    : pool_(pool),
      uploads_(uploads) {}

DynamicMeshLibrary::~DynamicMeshLibrary() {
    slots_.ForEach([this](const DynamicMeshHandle handle, const Slot&) { static_cast<void>(Destroy(handle)); });
}

Result<DynamicMeshHandle> DynamicMeshLibrary::Create(DynamicMeshDesc descriptor) {
    if (descriptor.vertex_capacity == 0 || descriptor.index_capacity == 0)
        return Err(ErrorCode::ValidationOutOfRange, "dynamic mesh capacities must be non-zero");
    BufferSlice vertices;
    TRY_ASSIGN(vertices, pool_.Allocate(descriptor.vertex_capacity, 16));
    auto index_result = pool_.Allocate(descriptor.index_capacity, 4);
    if (!index_result) {
        static_cast<void>(pool_.Free(vertices.allocation));
        return Err(std::move(index_result).error());
    }
    Slot slot;
    slot.record = {std::move(descriptor), vertices, *index_result, 1, {}};
    const auto handle = slots_.Emplace(std::move(slot));
    active_.push_back(handle);
    return Ok(handle);
}

Result<u64> DynamicMeshLibrary::Update(const DynamicMeshHandle handle, const std::span<const std::byte> vertices, const std::span<const std::byte> indices) {
    if (TryGet(handle) == nullptr)
        return Err(ErrorCode::InvalidState, "dynamic mesh handle is invalid");
    auto& record = slots_.Get(handle).record;
    if (vertices.size() > record.descriptor.vertex_capacity || indices.size() > record.descriptor.index_capacity)
        return Err(ErrorCode::ValidationOutOfRange, "dynamic mesh update exceeds capacity");
    std::vector<BufferUploadRequest> requests;
    if (!vertices.empty())
        requests.push_back({
            .target = pool_.SharedBuffer(),
            .offset = record.vertices.offset,
            .bytes = std::vector<std::byte>(vertices.begin(), vertices.end()),
            .publication = {},
        });
    if (!indices.empty())
        requests.push_back({
            .target = pool_.SharedBuffer(),
            .offset = record.indices.offset,
            .bytes = std::vector<std::byte>(indices.begin(), indices.end()),
            .publication = {},
        });
    if (!requests.empty())
        TRY_VOID(uploads_.EnqueueBatch(std::move(requests)));
    ++record.version;
    return Ok(record.version);
}

const DynamicMeshRecord* DynamicMeshLibrary::TryGet(const DynamicMeshHandle handle) const noexcept {
    const auto* slot = slots_.TryGet(handle);
    return slot == nullptr ? nullptr : &slot->record;
}

Result<void> DynamicMeshLibrary::Destroy(const DynamicMeshHandle handle) {
    if (TryGet(handle) == nullptr)
        return Err(ErrorCode::InvalidState, "dynamic mesh handle is invalid");
    auto& slot = slots_.Get(handle);
    TRY_VOID(pool_.Free(slot.record.vertices.allocation, slot.record.last_used));
    TRY_VOID(pool_.Free(slot.record.indices.allocation, slot.record.last_used));
    static_cast<void>(slots_.Remove(handle));
    std::erase(active_, handle);
    return Ok();
}

void DynamicMeshLibrary::MarkUsed(const DynamicMeshHandle handle, const rhi::SubmissionTicket submission) noexcept {
    if (TryGet(handle) != nullptr)
        slots_.Get(handle).record.last_used = submission;
}

} // namespace woki::gfx
