#include <algorithm>
#include <cmath>

#include <woki/gfx/advanced/temporal.hpp>
#include <woki/rhi/device.hpp>
#include <woki/rhi/queue.hpp>

namespace woki::gfx {
namespace {

bool SameDescriptor(const GraphTextureDesc& left, const GraphTextureDesc& right) {
    return left.extent.kind == right.extent.kind && left.extent.width == right.extent.width && left.extent.height == right.extent.height && left.depth_or_layers == right.depth_or_layers
           && left.mip_levels == right.mip_levels && left.sample_count == right.sample_count && left.dimension == right.dimension && left.format == right.format && left.usage == right.usage;
}

f32 Halton(u64 index, const u32 base) {
    f32 result{};
    f32 fraction = 1.0F;
    while (index != 0) {
        fraction /= static_cast<f32>(base);
        result += fraction * static_cast<f32>(index % base);
        index /= base;
    }
    return result;
}

} // namespace

RenderHistoryRegistry::RenderHistoryRegistry(ref<rhi::Device> device, ref<DeferredReleaseQueue> releases, const u32 slots)
    : device_(std::move(device)),
      releases_(std::move(releases)),
      slot_count_(std::clamp(slots, 2U, 3U)) {}

Result<RenderHistoryBinding> RenderHistoryRegistry::Acquire(const RenderHistoryKey key, const GraphTextureDesc& descriptor, const u64 frame, const bool invalidate) {
    if (device_ == nullptr || descriptor.extent.kind != ExtentKind::Fixed || descriptor.extent.width == 0 || descriptor.extent.height == 0)
        return Err(ErrorCode::InvalidArgument, "render history requires a device and a fixed non-empty extent");
    auto& entry = entries_[key];
    if (entry.slots.empty() || !SameDescriptor(entry.descriptor, descriptor)) {
        for (auto& slot : entry.slots) {
            slot.view.reset();
            if (slot.texture != nullptr && releases_ != nullptr)
                releases_->Retire(std::move(slot.texture), slot.last_use);
        }
        entry = {};
        entry.descriptor = descriptor;
        for (u32 index = 0; index < slot_count_; ++index) {
            Slot slot;
            TRY_ASSIGN(slot.texture, device_->CreateTexture({.size = {descriptor.extent.width, descriptor.extent.height, descriptor.depth_or_layers},
                                         .mip_level_count = descriptor.mip_levels,
                                         .sample_count = descriptor.sample_count,
                                         .dimension = descriptor.dimension,
                                         .format = descriptor.format,
                                         .usage = descriptor.usage,
                                         .view_formats = descriptor.view_formats,
                                         .label = descriptor.label + " history " + std::to_string(index)}));
            slot.view = ref<rhi::TextureView>(slot.texture
                    ->CreateView({.format = descriptor.format,
                        .mip_level_count = descriptor.mip_levels,
                        .array_layer_count = descriptor.depth_or_layers,
                        .aspect = descriptor.view_aspect,
                        .label = descriptor.label + " history view"})
                    .release());
            if (slot.view == nullptr)
                return Err(ErrorCode::GraphicsResourceCreationFailed, "failed to create render history view");
            entry.slots.push_back(std::move(slot));
        }
        entry.previous = 0;
        entry.current = 1;
    }
    if (invalidate)
        entry.valid = false;
    const auto completed = device_->GetQueue().CompletedSubmission();
    if (!completed.HasReached(entry.slots[entry.current].last_use)) {
        const Slot* previous_slot = &entry.slots[entry.previous];
        const auto available = std::ranges::find_if(entry.slots, [&](const Slot& slot) { return &slot != previous_slot && completed.HasReached(slot.last_use); });
        if (available == entry.slots.end())
            return Err(ErrorCode::QueueFull, "all render history slots are still in flight");
        entry.current = static_cast<u32>(std::distance(entry.slots.begin(), available));
    }
    entry.last_frame = frame;
    return Ok(RenderHistoryBinding{entry.slots[entry.previous].texture, entry.slots[entry.previous].view, entry.slots[entry.current].texture, entry.slots[entry.current].view, entry.valid});
}

void RenderHistoryRegistry::Submitted(const RenderHistoryKey key, const rhi::SubmissionTicket submission, const u64 frame) {
    const auto found = entries_.find(key);
    if (found == entries_.end() || !submission.IsValid())
        return;
    auto& entry = found->second;
    entry.slots[entry.current].last_use = submission;
    entry.previous = entry.current;
    entry.current = (entry.current + 1U) % static_cast<u32>(entry.slots.size());
    entry.last_frame = frame;
    entry.valid = true;
}

void RenderHistoryRegistry::Invalidate(const ViewHistoryId view) noexcept {
    for (auto& [key, entry] : entries_)
        if (key.view == view)
            entry.valid = false;
}

void RenderHistoryRegistry::Collect(const rhi::SubmissionEpoch completed, const u64 frame, const u32 maximum_age) {
    std::erase_if(entries_, [&](auto& pair) {
        if (pair.second.last_frame + maximum_age >= frame)
            return false;
        for (auto& slot : pair.second.slots) {
            if (!completed.HasReached(slot.last_use))
                return false;
            slot.view.reset();
            if (slot.texture != nullptr && releases_ != nullptr)
                releases_->Retire(std::move(slot.texture), slot.last_use);
        }
        return true;
    });
}

void RenderHistoryRegistry::MarkDeviceLost() noexcept {
    entries_.clear();
    device_.reset();
}

math::vec2f TemporalJitter(const u64 frame, const u32 render_width, const u32 render_height) noexcept {
    if (render_width == 0 || render_height == 0)
        return {};
    const u64 sample = frame % 16U + 1U;
    return {(Halton(sample, 2) - 0.5F) * 2.0F / static_cast<f32>(render_width), (Halton(sample, 3) - 0.5F) * 2.0F / static_cast<f32>(render_height)};
}

} // namespace woki::gfx
