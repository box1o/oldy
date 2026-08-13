#include <limits>
#include <algorithm>

#include <woki/rhi/objects.hpp>
#include <woki/gfx/advanced/buffer_pool.hpp>
#include "gfx_util.hpp"

namespace woki::gfx {
namespace {

[[nodiscard]] bool IsPowerOfTwo(const u64 value) noexcept {
    return value != 0 && (value & (value - 1)) == 0;
}

} // namespace

BufferRangeAllocator::BufferRangeAllocator(const u64 capacity) {
    Reset(capacity);
}

Result<BufferAllocation> BufferRangeAllocator::Allocate(const u64 size, const u64 alignment) {
    if (size == 0 || !IsPowerOfTwo(alignment))
        return Err(ErrorCode::ValidationOutOfRange, "buffer allocation size and alignment are invalid");
    size_t best = free_ranges_.size();
    u64 best_waste = std::numeric_limits<u64>::max();
    u64 best_offset{};
    for (size_t index = 0; index < free_ranges_.size(); ++index) {
        const auto& range = free_ranges_[index];
        const u64 aligned = detail::AlignUp(range.offset, alignment);
        if (aligned < range.offset || aligned - range.offset > range.size
            || size > range.size - (aligned - range.offset))
            continue;
        const u64 waste = range.size - size;
        if (waste < best_waste) {
            best = index;
            best_waste = waste;
            best_offset = aligned;
        }
    }
    if (best == free_ranges_.size())
        return Err(ErrorCode::FailedToAcquireResource, "buffer pool has no fitting range");

    const Range source = free_ranges_[best];
    free_ranges_.erase(free_ranges_.begin() + static_cast<std::ptrdiff_t>(best));
    if (best_offset > source.offset)
        InsertFree({source.offset, best_offset - source.offset});
    const u64 source_end = source.offset + source.size;
    if (best_offset + size < source_end)
        InsertFree({best_offset + size, source_end - best_offset - size});

    u32 slot_index{};
    if (free_slots_.empty()) {
        slot_index = static_cast<u32>(slots_.size());
        slots_.push_back({});
    } else {
        slot_index = free_slots_.back();
        free_slots_.pop_back();
    }
    auto& slot = slots_[slot_index];
    slot.range = {best_offset, size};
    slot.live = true;
    allocated_ += size;
    return Ok(BufferAllocation{MakeId(slot_index, slot.generation), best_offset, size});
}

Result<void> BufferRangeAllocator::Free(const AllocationId allocation) {
    if (!Contains(allocation))
        return Err(ErrorCode::ValidationInvalidState, "buffer allocation is stale or already freed");
    const u32 index = IdSlot(allocation);
    auto& slot = slots_[index];
    InsertFree(slot.range);
    allocated_ -= slot.range.size;
    slot.range = {};
    slot.live = false;
    ++slot.generation;
    if (slot.generation == 0)
        ++slot.generation;
    free_slots_.push_back(index);
    return Ok();
}

bool BufferRangeAllocator::Contains(const AllocationId allocation) const noexcept {
    const u32 index = IdSlot(allocation);
    return allocation.IsValid() && index < slots_.size() && slots_[index].live
           && slots_[index].generation == IdGeneration(allocation);
}

u64 BufferRangeAllocator::AllocationSize(const AllocationId allocation) const noexcept {
    return Contains(allocation) ? slots_[IdSlot(allocation)].range.size : 0;
}

BufferAllocatorStats BufferRangeAllocator::Stats() const noexcept {
    BufferAllocatorStats result{.capacity_bytes = capacity_,
        .allocated_bytes = allocated_,
        .free_bytes = capacity_ - allocated_,
        .free_range_count = static_cast<u32>(free_ranges_.size())};
    for (const auto& range : free_ranges_)
        result.largest_free_range = std::max(result.largest_free_range, range.size);
    if (result.free_bytes != 0)
        result.fragmentation = 1.0f - static_cast<f32>(result.largest_free_range) / static_cast<f32>(result.free_bytes);
    return result;
}

void BufferRangeAllocator::Reset(const u64 capacity) {
    capacity_ = capacity;
    allocated_ = 0;
    free_ranges_.clear();
    slots_.clear();
    free_slots_.clear();
    if (capacity != 0)
        free_ranges_.push_back({0, capacity});
}

AllocationId BufferRangeAllocator::MakeId(const u32 slot, const u32 generation) noexcept {
    return AllocationId((static_cast<u64>(generation) << 32) | (static_cast<u64>(slot) + 1));
}

u32 BufferRangeAllocator::IdSlot(const AllocationId id) noexcept {
    const u32 encoded = static_cast<u32>(id.Value());
    return encoded == 0 ? std::numeric_limits<u32>::max() : encoded - 1;
}

u32 BufferRangeAllocator::IdGeneration(const AllocationId id) noexcept {
    return static_cast<u32>(id.Value() >> 32);
}

void BufferRangeAllocator::InsertFree(Range range) {
    auto position = std::lower_bound(
        free_ranges_.begin(),
        free_ranges_.end(),
        range.offset,
        [](const Range& value, const u64 offset) { return value.offset < offset; }
    );
    position = free_ranges_.insert(position, range);
    if (position != free_ranges_.begin()) {
        auto previous = position - 1;
        if (previous->offset + previous->size == position->offset) {
            previous->size += position->size;
            position = free_ranges_.erase(position) - 1;
        }
    }
    if (position + 1 != free_ranges_.end() && position->offset + position->size == (position + 1)->offset) {
        position->size += (position + 1)->size;
        free_ranges_.erase(position + 1);
    }
}

Result<scope<BufferPool>> BufferPool::Create(
    ref<rhi::Device> device,
    BufferPoolDesc descriptor,
    ref<DeferredReleaseQueue> releases
) {
    if (device == nullptr || descriptor.size == 0 || !IsPowerOfTwo(descriptor.alignment))
        return Err(ErrorCode::ValidationInvalidState, "buffer pool descriptor is invalid");
    scope<rhi::Buffer> buffer;
    TRY_ASSIGN(
        buffer,
        device->CreateBuffer({.size = descriptor.size, .usage = descriptor.usage, .label = descriptor.label})
    );
    return Ok(
        scope<BufferPool>(new BufferPool(
            std::move(device),
            std::move(descriptor),
            ref<rhi::Buffer>(buffer.release()),
            std::move(releases)
        ))
    );
}

BufferPool::BufferPool(
    ref<rhi::Device> device,
    BufferPoolDesc descriptor,
    ref<rhi::Buffer> buffer,
    ref<DeferredReleaseQueue> releases
)
    : device_(std::move(device)),
      descriptor_(std::move(descriptor)),
      buffer_(std::move(buffer)),
      releases_(std::move(releases)),
      allocator_(descriptor_.size) {}

BufferPool::~BufferPool() {
    if (buffer_ != nullptr && releases_ != nullptr && last_used_.IsValid())
        releases_->Retire(std::move(buffer_), last_used_);
}

Result<BufferSlice> BufferPool::Allocate(const u64 size, u64 alignment) {
    if (residency_lost_ || buffer_ == nullptr)
        return Err(ErrorCode::GraphicsDeviceLost, "buffer pool residency is lost");
    if (alignment == 0)
        alignment = descriptor_.alignment;
    BufferAllocation allocation;
    TRY_ASSIGN(allocation, allocator_.Allocate(size, std::max(alignment, descriptor_.alignment)));
    return Ok(BufferSlice{allocation.id, buffer_.get(), allocation.offset, allocation.size});
}

Result<void> BufferPool::Free(const AllocationId allocation, const rhi::SubmissionTicket safe_after) {
    if (!allocator_.Contains(allocation))
        return Err(ErrorCode::ValidationInvalidState, "buffer pool allocation is stale or already freed");
    if (std::ranges::any_of(retired_, [&](const Retired& retired) { return retired.allocation == allocation; }))
        return Err(ErrorCode::ValidationInvalidState, "buffer pool allocation is already pending retirement");
    const u64 size = allocator_.AllocationSize(allocation);
    const rhi::SubmissionTicket retirement = safe_after.IsValid() ? safe_after : last_used_;
    if (!retirement.IsValid())
        return allocator_.Free(allocation);
    retired_.push_back({allocation, size, retirement});
    return Ok();
}

size_t BufferPool::Collect(const rhi::SubmissionEpoch completed) {
    const size_t before = retired_.size();
    std::erase_if(retired_, [&](const Retired& retired) {
        if (!completed.HasReached(retired.safe_after))
            return false;
        static_cast<void>(allocator_.Free(retired.allocation));
        return true;
    });
    return before - retired_.size();
}

void BufferPool::MarkUsed(const rhi::SubmissionTicket submission) noexcept {
    if (submission.IsValid() && submission.Value() > last_used_.Value())
        last_used_ = submission;
}

void BufferPool::MarkResidencyLost() noexcept {
    residency_lost_ = true;
}

rhi::Buffer& BufferPool::Buffer() const noexcept {
    return *buffer_;
}

ref<rhi::Buffer> BufferPool::SharedBuffer() const noexcept {
    return buffer_;
}

const BufferPoolDesc& BufferPool::Descriptor() const noexcept {
    return descriptor_;
}

BufferPoolStats BufferPool::Stats() const noexcept {
    u64 pending{};
    for (const auto& retired : retired_)
        pending += retired.size;
    return {.pool_class = descriptor_.pool_class,
        .allocator = allocator_.Stats(),
        .estimated_bytes = descriptor_.size,
        .pending_free_bytes = pending,
        .last_used = last_used_,
        .residency_lost = residency_lost_};
}

BufferPoolDesc DefaultBufferPoolDesc(const BufferPoolClass pool_class, const u64 size) {
    BufferPoolDesc result{.pool_class = pool_class, .size = size};
    switch (pool_class) {
        case BufferPoolClass::Vertex:
            result.usage = rhi::BufferUsage::Vertex | rhi::BufferUsage::CopyDst;
            break;
        case BufferPoolClass::Index:
            result.usage = rhi::BufferUsage::Index | rhi::BufferUsage::CopyDst;
            break;
        case BufferPoolClass::Indirect:
            result.usage = rhi::BufferUsage::Indirect | rhi::BufferUsage::Storage | rhi::BufferUsage::CopyDst;
            break;
        case BufferPoolClass::Upload:
            result.usage = rhi::BufferUsage::MapWrite | rhi::BufferUsage::CopySrc;
            break;
        case BufferPoolClass::Readback:
            result.usage = rhi::BufferUsage::MapRead | rhi::BufferUsage::CopyDst;
            break;
        case BufferPoolClass::Material:
            result.usage = rhi::BufferUsage::Uniform | rhi::BufferUsage::CopyDst;
            break;
        default:
            result.usage = rhi::BufferUsage::Storage | rhi::BufferUsage::CopyDst;
            break;
    }
    return result;
}

} // namespace woki::gfx
