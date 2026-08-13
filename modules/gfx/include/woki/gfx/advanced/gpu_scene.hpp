#pragma once

#include <array>

#include <woki/core.hpp>
#include <woki/math.hpp>

#include "buffer_pool.hpp"
#include "render_abi.hpp"
#include "render_world.hpp"
#include "upload.hpp"
#include "visibility.hpp"

namespace woki::gfx {

using GpuInstanceRecord = abi::GpuInstanceRecord;
using GpuLightRecord = abi::GpuLightRecord;

[[nodiscard]] GpuInstanceRecord PackGpuInstance(const RenderWorldSnapshot& world, u32 dense_index, math::vec3f camera_origin, u32 gpu_version) noexcept;
[[nodiscard]] GpuLightRecord PackGpuLight(const RenderLightData& light, math::vec3f camera_origin) noexcept;

class GpuSceneInstance final {
public:
    [[nodiscard]] u32 Version() const noexcept {
        return version_;
    }

    [[nodiscard]] u32 Index() const noexcept {
        return index_;
    }

private:
    friend class GpuScene;

    GpuSceneInstance(u32 index, u32 version) noexcept
        : index_(index),
          version_(version) {}

    u32 index_{};
    u32 version_{};
};

struct GpuSceneStats final {
    u32 instances{};
    u32 lights{};
    u32 capacity{};
    u64 allocation_version{};
    size_t retired_objects{};
    bool device_lost{};
};

struct GpuSceneTables final {
    BufferSlice instances;
    BufferSlice transforms;
    BufferSlice lights;
    BufferSlice materials;
    u64 generation{};
};

class GpuScene final {
public:
    GpuScene(BufferPool& pool, UploadScheduler& uploads, u32 initial_capacity = 256, f32 whole_upload_threshold = 0.6F);
    ~GpuScene();
    GpuScene(const GpuScene&) = delete;
    GpuScene& operator=(const GpuScene&) = delete;

    [[nodiscard]] Result<void> Synchronize(const RenderWorldSnapshot& world, math::vec3f camera_origin = {});
    [[nodiscard]] Result<void> UploadDirty();
    [[nodiscard]] std::optional<GpuSceneInstance> Resolve(RenderObjectId id) const noexcept;
    void MarkUsed(rhi::SubmissionTicket submission) noexcept;
    void Collect(rhi::SubmissionEpoch completed);
    void MarkDeviceLost() noexcept;
    [[nodiscard]] GpuSceneStats Stats() const noexcept;
    [[nodiscard]] GpuSceneTables Tables() const noexcept;

private:
    struct Mapping {
        u32 generation{};
        u32 version{};
        u64 source_version{};
    };

    struct Retired {
        RenderObjectId id;
        rhi::SubmissionTicket safe_after;
    };

    [[nodiscard]] Result<void> EnsureCapacity(u32 instances, u32 lights);
    void Remove(RenderObjectId id);

    BufferPool* pool_;
    UploadScheduler* uploads_;
    BufferSlice instances_allocation_;
    BufferSlice transforms_allocation_;
    BufferSlice lights_allocation_;
    BufferSlice materials_allocation_;
    DenseHandleStorage<GpuInstanceRecord, RenderObjectId> instances_;
    std::vector<GpuLightRecord> lights_;
    std::vector<Mapping> mappings_;
    std::vector<Retired> retired_;
    DirtyRangeSet instance_dirty_;
    DirtyRangeSet light_dirty_;
    rhi::SubmissionTicket last_used_;
    u32 capacity_{};
    u32 light_capacity_{};
    u64 allocation_version_{};
    u32 next_mapping_version_{1};
    u32 initial_capacity_{};
    f32 whole_upload_threshold_;
    math::vec3f camera_origin_{};
    bool has_camera_origin_{};
    bool device_lost_{};
};

struct RenderWorldServices final {
    RenderWorldServices(BufferPool& pool, UploadScheduler& uploads, u32 initial_gpu_capacity = 256)
        : gpu_scene(createScope<GpuScene>(pool, uploads, initial_gpu_capacity)) {}

    RenderScene scene;
    RenderWorldBuilder world;
    VisibilityService visibility;
    scope<GpuScene> gpu_scene;
};

} // namespace woki::gfx
