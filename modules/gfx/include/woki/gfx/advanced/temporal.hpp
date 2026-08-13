#pragma once

#include <map>

#include <woki/math.hpp>
#include <woki/rhi/objects.hpp>

#include "deferred_release.hpp"
#include "render_graph.hpp"
#include "../view.hpp"

namespace woki::gfx {

enum class TemporalSemantic : u8 { HdrColor, Depth, Velocity, Exposure, HiZDepth };

struct RenderHistoryKey final {
    ViewHistoryId view;
    TemporalSemantic semantic{TemporalSemantic::HdrColor};
    [[nodiscard]] friend auto operator<=>(const RenderHistoryKey&, const RenderHistoryKey&) = default;
};

struct RenderHistoryBinding final {
    ref<rhi::Texture> previous;
    ref<rhi::TextureView> previous_view;
    ref<rhi::Texture> current;
    ref<rhi::TextureView> current_view;
    bool valid{};
};

// Slots rotate only after successful submission and are retired only after the
// queue completion watermark proves their last use safe.
class RenderHistoryRegistry final {
public:
    RenderHistoryRegistry(ref<rhi::Device> device, ref<DeferredReleaseQueue> releases, u32 slots = 3);
    [[nodiscard]] Result<RenderHistoryBinding> Acquire(RenderHistoryKey key, const GraphTextureDesc& descriptor, u64 frame, bool invalidate);
    void Submitted(RenderHistoryKey key, rhi::SubmissionTicket submission, u64 frame);
    void Invalidate(ViewHistoryId view) noexcept;
    void Collect(rhi::SubmissionEpoch completed, u64 frame, u32 maximum_age);
    void MarkDeviceLost() noexcept;

private:
    struct Slot final {
        ref<rhi::Texture> texture;
        ref<rhi::TextureView> view;
        rhi::SubmissionTicket last_use;
    };

    struct Entry final {
        GraphTextureDesc descriptor;
        std::vector<Slot> slots;
        u32 previous{};
        u32 current{1};
        u64 last_frame{};
        bool valid{};
    };

    ref<rhi::Device> device_;
    ref<DeferredReleaseQueue> releases_;
    u32 slot_count_{3};
    std::map<RenderHistoryKey, Entry> entries_;
};

[[nodiscard]] math::vec2f TemporalJitter(u64 frame, u32 render_width, u32 render_height) noexcept;

} // namespace woki::gfx
