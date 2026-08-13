#include <algorithm>
#include <cmath>

#include <woki/task.hpp>
#include <woki/gfx/advanced/visibility.hpp>

namespace woki::gfx {
namespace {

FrustumPlane Normalize(const f32 a, const f32 b, const f32 c, const f32 d) noexcept {
    const f32 length = std::sqrt(a * a + b * b + c * c);
    if (length <= 1.0e-8F)
        return {};
    return {{a / length, b / length, c / length}, d / length};
}

f32 PlaneDistance(const FrustumPlane& plane, const math::vec3f& point) noexcept {
    return plane.normal.x * point.x + plane.normal.y * point.y + plane.normal.z * point.z + plane.distance;
}

bool Flagged(const RenderObjectFlags value, const RenderObjectFlags flag) noexcept {
    return (static_cast<u32>(value) & static_cast<u32>(flag)) != 0;
}

RenderPhase SurfacePhase(const MaterialPhase phase, const RenderObjectFlags flags) noexcept {
    if (Flagged(flags, RenderObjectFlags::Overlay))
        return RenderPhase::Overlay;
    switch (phase) {
        case MaterialPhase::Opaque:
            return RenderPhase::Opaque;
        case MaterialPhase::AlphaTest:
            return RenderPhase::AlphaTest;
        case MaterialPhase::Transparent:
            return RenderPhase::Transparent;
    }
    return RenderPhase::Opaque;
}

u32 SelectLod(const RenderLodState& lod, const f32 projected_scale, const f32 threshold, const f32 hysteresis) noexcept {
    const u32 count = std::clamp(lod.count, 1U, RenderLodState::kMaxLods);
    u32 requested = count - 1;
    for (u32 index = 0; index < count; ++index) {
        if (lod.geometric_errors[index] * projected_scale <= threshold) {
            requested = index;
            break;
        }
    }
    if (lod.previous < count && requested != lod.previous) {
        const f32 boundary = lod.geometric_errors[std::min(requested, lod.previous)] * projected_scale;
        if (std::abs(boundary - threshold) <= threshold * hysteresis)
            requested = lod.previous;
    }
    const u32 first = std::min(lod.first_resident, count - 1);
    const u32 last = std::clamp(lod.last_resident, first, count - 1);
    return std::clamp(requested, first, last);
}

} // namespace

Frustum Frustum::FromWebGpuViewProjection(const math::mat4f& matrix) noexcept {
    Frustum result;
    auto row = [&](const u32 r, const u32 c) { return matrix(r, c); };
    result.planes_[0] = Normalize(row(3, 0) + row(0, 0), row(3, 1) + row(0, 1), row(3, 2) + row(0, 2), row(3, 3) + row(0, 3));
    result.planes_[1] = Normalize(row(3, 0) - row(0, 0), row(3, 1) - row(0, 1), row(3, 2) - row(0, 2), row(3, 3) - row(0, 3));
    result.planes_[2] = Normalize(row(3, 0) + row(1, 0), row(3, 1) + row(1, 1), row(3, 2) + row(1, 2), row(3, 3) + row(1, 3));
    result.planes_[3] = Normalize(row(3, 0) - row(1, 0), row(3, 1) - row(1, 1), row(3, 2) - row(1, 2), row(3, 3) - row(1, 3));
    result.planes_[4] = Normalize(row(2, 0), row(2, 1), row(2, 2), row(2, 3));
    result.planes_[5] = Normalize(row(3, 0) - row(2, 0), row(3, 1) - row(2, 1), row(3, 2) - row(2, 2), row(3, 3) - row(2, 3));
    return result;
}

bool Frustum::IntersectsSphere(const math::vec3f& center, const f32 radius) const noexcept {
    return std::ranges::all_of(planes_, [&](const FrustumPlane& plane) { return PlaneDistance(plane, center) >= -std::max(0.0F, radius); });
}

bool Frustum::IntersectsAabb(const math::vec3f& minimum, const math::vec3f& maximum) const noexcept {
    return std::ranges::all_of(planes_, [&](const FrustumPlane& plane) {
        const math::vec3f positive{
            plane.normal.x >= 0.0F ? maximum.x : minimum.x,
            plane.normal.y >= 0.0F ? maximum.y : minimum.y,
            plane.normal.z >= 0.0F ? maximum.z : minimum.z,
        };
        return PlaneDistance(plane, positive) >= 0.0F;
    });
}

Result<VisibilityResult> VisibilityService::Cull(const RenderWorldSnapshot& world, const RenderView& view, VisibilityOptions options, task::Scheduler* scheduler, const OcclusionVisibilityHook* occlusion) const {
    const auto& objects = world.ObjectData();
    options.batch_size = std::max<size_t>(1, options.batch_size);
    options.lod_error_pixels = std::max(0.0F, options.lod_error_pixels);
    options.lod_hysteresis = std::max(0.0F, options.lod_hysteresis);
    const size_t batch_count = (objects.ids.size() + options.batch_size - 1) / options.batch_size;
    std::vector<std::vector<VisibleObject>> batches(batch_count);
    const Frustum frustum = Frustum::FromWebGpuViewProjection(view.camera.view_projection);

    auto process = [&](const size_t batch) {
        const size_t first = batch * options.batch_size;
        const size_t last = std::min(objects.ids.size(), first + options.batch_size);
        auto& output = batches[batch];
        output.reserve(last - first);
        for (size_t index = first; index < last; ++index) {
            if (Flagged(objects.flags[index], RenderObjectFlags::Hidden) || (objects.visibility_masks[index] & view.visibility_mask) == 0 || (objects.layers[index] & view.layer_mask) == 0)
                continue;
            const auto& bounds = objects.bounds[index];
            if (!frustum.IntersectsSphere(bounds.center, bounds.radius) || !frustum.IntersectsAabb(bounds.minimum, bounds.maximum))
                continue;
            if (occlusion != nullptr && !HasFlag(view.flags, ViewFlags::DisableOcclusion) && !occlusion->PotentiallyVisible(objects.ids[index], bounds, view))
                continue;
            const f32 dx = bounds.center.x - view.camera.position.x;
            const f32 dy = bounds.center.y - view.camera.position.y;
            const f32 dz = bounds.center.z - view.camera.position.z;
            const f32 distance = std::max(std::sqrt(dx * dx + dy * dy + dz * dz), 1.0e-4F);
            const f32 projected_scale = static_cast<f32>(view.viewport.height) * 0.5F * std::abs(view.camera.projection(1, 1)) / distance;
            const f32 view_z = view.camera.view(2, 0) * bounds.center.x + view.camera.view(2, 1) * bounds.center.y + view.camera.view(2, 2) * bounds.center.z + view.camera.view(2, 3);
            output.push_back({objects.ids[index], static_cast<u32>(index), 0, SelectLod(objects.lods[index], projected_scale, options.lod_error_pixels, options.lod_hysteresis),
                SurfacePhase(objects.material_phases[index], objects.flags[index]), -view_z});
        }
    };

    if (scheduler != nullptr && batch_count > 1) {
#ifdef __EMSCRIPTEN__
        for (size_t batch = 0; batch < batch_count; ++batch)
            process(batch);
#else
        auto completed = task::ParallelFor(*scheduler, 0, batch_count, process, {}, 1, batch_count).Wait();
        if (!completed)
            return Err(std::move(completed).error());
#endif
    } else {
        for (size_t batch = 0; batch < batch_count; ++batch)
            process(batch);
    }

    VisibilityResult result;
    for (auto& batch : batches)
        result.visible.insert(result.visible.end(), std::make_move_iterator(batch.begin()), std::make_move_iterator(batch.end()));
    for (size_t source_index = 0; source_index < result.visible.size(); ++source_index) {
        auto& item = result.visible[source_index];
        item.source_index = static_cast<u32>(source_index);
        result.phases[static_cast<size_t>(item.phase)].push_back(item);
        if (item.phase == RenderPhase::Opaque || item.phase == RenderPhase::AlphaTest)
            result.phases[static_cast<size_t>(RenderPhase::Depth)].push_back(item);
        if (Flagged(objects.flags[item.dense_index], RenderObjectFlags::CastShadow) && item.phase != RenderPhase::Overlay)
            result.phases[static_cast<size_t>(RenderPhase::Shadow)].push_back(item);
    }
    auto stable_id = [](const VisibleObject& left, const VisibleObject& right) { return left.id < right.id; };
    for (auto& phase : result.phases)
        std::stable_sort(phase.begin(), phase.end(), stable_id);
    auto& transparent = result.phases[static_cast<size_t>(RenderPhase::Transparent)];
    std::stable_sort(transparent.begin(), transparent.end(), [](const VisibleObject& left, const VisibleObject& right) {
        if (left.view_depth != right.view_depth)
            return left.view_depth > right.view_depth;
        return left.id < right.id;
    });
    return Ok(std::move(result));
}

Result<PreparedPhaseDraws> PrepareDrawPackets(const VisibilityResult& visibility, const std::span<const DrawPacketMesh> sources, const DrawPacketBuilder& builder) {
    if (sources.size() != visibility.visible.size())
        return Err(ErrorCode::ValidationOutOfRange, "draw preparation sources do not match visible objects");
    PreparedPhaseDraws result;
    for (size_t phase_index = 0; phase_index < result.phases.size(); ++phase_index) {
        const auto& visible_phase = visibility.phases[phase_index];
        std::vector<DrawPacketMesh> selected;
        selected.reserve(visible_phase.size());
        for (const auto& object : visible_phase) {
            if (object.source_index >= sources.size() || visibility.visible[object.source_index].id != object.id)
                return Err(ErrorCode::ValidationInvalidState, "phase contains an object absent from the visible list");
            selected.push_back(sources[object.source_index]);
        }
        if (phase_index == static_cast<size_t>(RenderPhase::Transparent)) {
            for (const auto& source : selected) {
                std::vector<DrawPacket> packets;
                TRY_ASSIGN(packets, builder.Build(std::span(&source, 1)));
                result.phases[phase_index].insert(result.phases[phase_index].end(), std::make_move_iterator(packets.begin()), std::make_move_iterator(packets.end()));
            }
        } else {
            TRY_ASSIGN(result.phases[phase_index], builder.Build(std::span<const DrawPacketMesh>(selected)));
        }
    }
    return Ok(std::move(result));
}

} // namespace woki::gfx
