#pragma once

#include "upload.hpp"
#include "buffer_pool.hpp"
#include "deferred_release.hpp"

namespace woki::gfx {

class ReadbackManager;

struct RenderRuntimeDesc final {
    u32 frames_in_flight{3};
    u64 frame_scratch_bytes{8ull * 1024ull * 1024ull};
    UploadBudget upload_budget;
    u64 readback_budget_bytes{32ull * 1024ull * 1024ull};
    u32 readback_request_budget{256};
    std::vector<BufferPoolDesc> buffer_pools;
};

struct RenderRuntimeDiagnostics final {
    FrameEpoch next_frame;
    UploadStats uploads;
    std::vector<BufferPoolStats> pools;
    size_t deferred_releases{};
    bool device_lost{};
};

class RenderRuntimeServices final {
public:
    [[nodiscard]] static Result<scope<RenderRuntimeServices>> Create(ref<rhi::Device> device, RenderRuntimeDesc descriptor = {});
    ~RenderRuntimeServices();

    RenderRuntimeServices(const RenderRuntimeServices&) = delete;
    RenderRuntimeServices& operator=(const RenderRuntimeServices&) = delete;

    [[nodiscard]] Result<std::reference_wrapper<FrameContext>> AcquireFrame();
    void Collect();
    void MarkDeviceLost() noexcept;

    [[nodiscard]] FrameContextRing& Frames() noexcept {
        return frames_;
    }

    [[nodiscard]] UploadScheduler& Uploads() noexcept {
        return uploads_;
    }

    [[nodiscard]] ReadbackManager& Readbacks() noexcept {
        return *readbacks_;
    }

    [[nodiscard]] ref<DeferredReleaseQueue> Releases() const noexcept {
        return releases_;
    }

    [[nodiscard]] BufferPool* Pool(BufferPoolClass pool_class) noexcept;
    [[nodiscard]] RenderRuntimeDiagnostics Diagnostics() const;

private:
    RenderRuntimeServices(ref<rhi::Device> device, const RenderRuntimeDesc& descriptor);

    ref<rhi::Device> device_;
    FrameContextRing frames_;
    UploadScheduler uploads_;
    scope<ReadbackManager> readbacks_;
    ref<DeferredReleaseQueue> releases_;
    std::vector<scope<BufferPool>> pools_;
    bool device_lost_{};
};

} // namespace woki::gfx
