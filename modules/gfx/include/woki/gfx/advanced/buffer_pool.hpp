#pragma once

#include <string>
#include <vector>

#include <woki/rhi/device.hpp>

#include "epoch.hpp"
#include "deferred_release.hpp"

namespace woki::gfx {

struct BufferAllocation final {
    AllocationId id;
    u64 offset{};
    u64 size{};
};

struct BufferAllocatorStats final {
    u64 capacity_bytes{};
    u64 allocated_bytes{};
    u64 free_bytes{};
    u64 largest_free_range{};
    u32 free_range_count{};
    f32 fragmentation{};
};

class BufferRangeAllocator final {
public:
    explicit BufferRangeAllocator(u64 capacity = 0);
    [[nodiscard]] Result<BufferAllocation> Allocate(u64 size, u64 alignment = 1);
    [[nodiscard]] Result<void> Free(AllocationId allocation);
    [[nodiscard]] bool Contains(AllocationId allocation) const noexcept;
    [[nodiscard]] u64 AllocationSize(AllocationId allocation) const noexcept;
    [[nodiscard]] BufferAllocatorStats Stats() const noexcept;
    void Reset(u64 capacity);

private:
    struct Range final {
        u64 offset{};
        u64 size{};
    };

    struct Slot final {
        u32 generation{1};
        Range range;
        bool live{};
    };

    [[nodiscard]] static AllocationId MakeId(u32 slot, u32 generation) noexcept;
    [[nodiscard]] static u32 IdSlot(AllocationId id) noexcept;
    [[nodiscard]] static u32 IdGeneration(AllocationId id) noexcept;
    void InsertFree(Range range);

    u64 capacity_{};
    u64 allocated_{};
    std::vector<Range> free_ranges_;
    std::vector<Slot> slots_;
    std::vector<u32> free_slots_;
};

enum class BufferPoolClass : u8 {
    Vertex,
    Index,
    Material,
    GpuScene,
    Skinning,
    Indirect,
    Dynamic,
    Upload,
    Readback,
};

struct BufferPoolDesc final {
    BufferPoolClass pool_class{BufferPoolClass::Vertex};
    u64 size{};
    u64 alignment{16};
    rhi::BufferUsage usage{rhi::BufferUsage::CopyDst};
    std::string label{"BufferPool"};
};

struct BufferSlice final {
    AllocationId allocation;
    rhi::Buffer* buffer{};
    u64 offset{};
    u64 size{};
};

struct BufferPoolStats final {
    BufferPoolClass pool_class{};
    BufferAllocatorStats allocator;
    u64 estimated_bytes{};
    u64 pending_free_bytes{};
    rhi::SubmissionTicket last_used;
    bool residency_lost{};
};

class BufferPool final {
public:
    [[nodiscard]] static Result<scope<BufferPool>> Create(ref<rhi::Device> device, BufferPoolDesc descriptor, ref<DeferredReleaseQueue> releases = {});
    ~BufferPool();

    BufferPool(const BufferPool&) = delete;
    BufferPool& operator=(const BufferPool&) = delete;

    [[nodiscard]] Result<BufferSlice> Allocate(u64 size, u64 alignment = 0);
    [[nodiscard]] Result<void> Free(AllocationId allocation, rhi::SubmissionTicket safe_after = {});
    size_t Collect(rhi::SubmissionEpoch completed);
    void MarkUsed(rhi::SubmissionTicket submission) noexcept;
    void MarkResidencyLost() noexcept;

    [[nodiscard]] rhi::Buffer& Buffer() const noexcept;
    [[nodiscard]] ref<rhi::Buffer> SharedBuffer() const noexcept;
    [[nodiscard]] const BufferPoolDesc& Descriptor() const noexcept;
    [[nodiscard]] BufferPoolStats Stats() const noexcept;

private:
    struct Retired final {
        AllocationId allocation;
        u64 size{};
        rhi::SubmissionTicket safe_after;
    };

    BufferPool(ref<rhi::Device> device, BufferPoolDesc descriptor, ref<rhi::Buffer> buffer, ref<DeferredReleaseQueue> releases);

    ref<rhi::Device> device_;
    BufferPoolDesc descriptor_;
    ref<rhi::Buffer> buffer_;
    ref<DeferredReleaseQueue> releases_;
    BufferRangeAllocator allocator_;
    std::vector<Retired> retired_;
    rhi::SubmissionTicket last_used_;
    bool residency_lost_{};
};

[[nodiscard]] BufferPoolDesc DefaultBufferPoolDesc(BufferPoolClass pool_class, u64 size);

} // namespace woki::gfx
