#include "../internal/runtime_state.hpp"

namespace woki::gfx {

Result<SurfaceHandle> RenderRuntime::CreateSurface(SurfaceDescriptor descriptor) {
    if (descriptor.source == nullptr)
        return Err(ErrorCode::InvalidArgument, "surface source is required");
    u32 index{};
    if (impl_->free_surfaces.empty()) {
        index = static_cast<u32>(impl_->surface_slots.size());
        impl_->surface_slots.emplace_back();
    } else {
        index = impl_->free_surfaces.back();
        impl_->free_surfaces.pop_back();
    }
    auto& slot = impl_->surface_slots[index];
    TRY_ASSIGN(slot.surface, CreatePlatformSurface(*impl_->instance, *descriptor.source));
    slot.descriptor = std::move(descriptor);
    if (slot.descriptor.width == 0 || slot.descriptor.height == 0)
        slot.state = SurfaceState::Minimized;
    else {
        auto built = rhi::Swapchain::Builder(impl_->device, slot.surface)
                         .Size(slot.descriptor.width, slot.descriptor.height)
                         .PresentMode(ToRhiPresentMode(slot.descriptor.present_mode))
                         .Label(slot.descriptor.label.empty() ? slot.descriptor.source->Label() : slot.descriptor.label)
                         .Build();
        if (!built) {
            slot.surface.reset();
            impl_->free_surfaces.push_back(index);
            return Err(std::move(built).error());
        }
        slot.swapchain = std::move(*built);
        slot.state = SurfaceState::Ready;
    }
    return Ok(SurfaceHandle::Create(index, slot.generation));
}

Result<void> RenderRuntime::ReconfigureSurface(const SurfaceHandle surface, const u32 width, const u32 height) {
    if (!surface.IsValid() || surface.Index() >= impl_->surface_slots.size())
        return Err(ErrorCode::InvalidArgument, "surface handle is stale");
    auto& slot = impl_->surface_slots[surface.Index()];
    if (slot.generation != surface.Generation() || slot.surface == nullptr)
        return Err(ErrorCode::InvalidArgument, "surface handle is stale");
    slot.acquired.reset();
    if (width == 0 || height == 0) {
        slot.descriptor.width = width;
        slot.descriptor.height = height;
        slot.state = SurfaceState::Minimized;
        return Ok();
    }
    if (slot.swapchain == nullptr) {
        scope<rhi::Swapchain> replacement;
        TRY_ASSIGN(
            replacement,
            rhi::Swapchain::Builder(impl_->device, slot.surface)
                .Size(width, height)
                .PresentMode(ToRhiPresentMode(slot.descriptor.present_mode))
                .Label(slot.descriptor.label)
                .Build()
        );
        slot.swapchain = std::move(replacement);
    } else {
        slot.swapchain->Resize(width, height);
    }
    slot.descriptor.width = width;
    slot.descriptor.height = height;
    slot.state = SurfaceState::Ready;
    impl_->executables.clear();
    return Ok();
}

Result<void> RenderRuntime::DestroySurface(const SurfaceHandle surface) {
    if (!surface.IsValid() || surface.Index() >= impl_->surface_slots.size())
        return Err(ErrorCode::InvalidArgument, "surface handle is stale");
    auto& slot = impl_->surface_slots[surface.Index()];
    if (slot.generation != surface.Generation() || slot.surface == nullptr)
        return Err(ErrorCode::InvalidArgument, "surface handle is stale");
    slot.acquired.reset();
    if (slot.swapchain != nullptr)
        impl_->resources->Releases()->Retire(std::move(slot.swapchain), slot.last_used);
    if (slot.surface != nullptr)
        impl_->resources->Releases()->Retire(std::move(slot.surface), slot.last_used);
    ++slot.generation;
    if (slot.generation == 0)
        ++slot.generation;
    impl_->free_surfaces.push_back(surface.Index());
    return Ok();
}

SurfaceState RenderRuntime::GetSurfaceState(const SurfaceHandle surface) const noexcept {
    if (!surface.IsValid() || surface.Index() >= impl_->surface_slots.size())
        return SurfaceState::Lost;
    const auto& slot = impl_->surface_slots[surface.Index()];
    return slot.generation == surface.Generation() && slot.surface != nullptr ? slot.state : SurfaceState::Lost;
}

std::vector<SurfaceHandle> RenderRuntime::Surfaces() const {
    std::vector<SurfaceHandle> result;
    for (u32 index = 0; index < impl_->surface_slots.size(); ++index)
        if (const auto& slot = impl_->surface_slots[index]; slot.surface != nullptr)
            result.push_back(SurfaceHandle::Create(index, slot.generation));
    return result;
}

Result<OffscreenTargetHandle> RenderRuntime::CreateOffscreenTarget(OffscreenTargetDescriptor descriptor) {
    if (descriptor.width == 0 || descriptor.height == 0)
        return Err(ErrorCode::InvalidArgument, "offscreen target dimensions must be non-zero");
    u32 index{};
    if (impl_->free_offscreens.empty()) {
        index = static_cast<u32>(impl_->offscreen_slots.size());
        impl_->offscreen_slots.emplace_back();
    } else {
        index = impl_->free_offscreens.back();
        impl_->free_offscreens.pop_back();
    }
    auto& slot = impl_->offscreen_slots[index];
    slot.descriptor = std::move(descriptor);
    TRY_VOID(impl_->RestoreOffscreen(slot));
    return Ok(OffscreenTargetHandle::Create(index, slot.generation));
}

Result<void> RenderRuntime::ResizeOffscreenTarget(
    const OffscreenTargetHandle target,
    const u32 width,
    const u32 height
) {
    if (!target.IsValid() || target.Index() >= impl_->offscreen_slots.size() || width == 0 || height == 0)
        return Err(ErrorCode::InvalidArgument, "offscreen target is stale or dimensions are zero");
    auto& slot = impl_->offscreen_slots[target.Index()];
    if (slot.generation != target.Generation() || !slot.descriptor)
        return Err(ErrorCode::InvalidArgument, "offscreen target handle is stale");
    if (slot.descriptor->width == width && slot.descriptor->height == height)
        return Ok();
    auto next_descriptor = *slot.descriptor;
    next_descriptor.width = width;
    next_descriptor.height = height;
    Impl::OffscreenSlot replacement;
    replacement.descriptor = next_descriptor;
    TRY_VOID(impl_->RestoreOffscreen(replacement));
    if (slot.view != nullptr)
        impl_->resources->Releases()->Retire(std::move(slot.view), slot.last_used);
    if (slot.texture != nullptr)
        impl_->resources->Releases()->Retire(std::move(slot.texture), slot.last_used);
    slot.descriptor = std::move(next_descriptor);
    slot.texture = std::move(replacement.texture);
    slot.view = std::move(replacement.view);
    ++slot.version;
    for (auto& view : impl_->view_slots) {
        if (!view.descriptor)
            continue;
        const auto* output = std::get_if<OffscreenOutput>(&view.descriptor->output);
        if (output && output->target == target) {
            view.descriptor->flags = view.descriptor->flags | ViewFlags::Resized;
            if (impl_->histories)
                impl_->histories->Invalidate(view.history);
        }
    }
    impl_->executables.clear();
    return Ok();
}

Result<void> RenderRuntime::DestroyOffscreenTarget(const OffscreenTargetHandle target) {
    if (!target.IsValid() || target.Index() >= impl_->offscreen_slots.size())
        return Err(ErrorCode::InvalidArgument, "offscreen target handle is stale");
    auto& slot = impl_->offscreen_slots[target.Index()];
    if (slot.generation != target.Generation() || !slot.descriptor)
        return Err(ErrorCode::InvalidArgument, "offscreen target handle is stale");
    if (slot.view != nullptr)
        impl_->resources->Releases()->Retire(std::move(slot.view), slot.last_used);
    if (slot.texture != nullptr)
        impl_->resources->Releases()->Retire(std::move(slot.texture), slot.last_used);
    slot.descriptor.reset();
    ++slot.generation;
    if (slot.generation == 0)
        ++slot.generation;
    impl_->free_offscreens.push_back(target.Index());
    return Ok();
}

Result<FrameResult> RenderRuntime::RenderFrame(const RenderFrameRequest& request) {
    if (impl_->shutdown)
        return Err(ErrorCode::InvalidState, "render runtime is shut down");
    if (request.canvas && !request.canvas->Valid())
        return Err(ErrorCode::InvalidArgument, "canvas frame is invalid");
    std::vector<ViewId> selected;
    if (!request.views.empty())
        selected.assign(request.views.begin(), request.views.end());
    else
        for (u32 index = 0; index < impl_->view_slots.size(); ++index)
            if (const auto& slot = impl_->view_slots[index]; slot.descriptor && slot.descriptor->active)
                selected.push_back(ViewId::Create(index, slot.generation));

    FrameResult result;
    std::map<u64, RenderViewFamily> families;
    std::vector<FrameOutputBinding> outputs;
    std::set<u32> acquired_surfaces;
    std::optional<CanvasTarget> canvas_target;
    if (request.canvas) {
        const auto handle = request.canvas->surface;
        if (!handle.IsValid() || handle.Index() >= impl_->surface_slots.size())
            return Err(ErrorCode::InvalidArgument, "canvas references a stale surface");
        auto& surface = impl_->surface_slots[handle.Index()];
        if (surface.generation != handle.Generation() || surface.state != SurfaceState::Ready)
            return Err(ErrorCode::InvalidState, "canvas surface is not ready");
        auto acquired = surface.swapchain->AcquireNextFrame();
        if (!acquired)
            return Err(std::move(acquired).error());
        surface.acquired = std::move(*acquired);
        acquired_surfaces.insert(handle.Index());
        canvas_target = CanvasTarget{
            .texture = {},
            .view = surface.acquired->ColorViewRef(),
            .format = surface.swapchain->ColorFormat(),
            .width = surface.acquired->Width(),
            .height = surface.acquired->Height(),
        };
    }
    std::set<SceneHandle> selected_scenes(request.scenes.begin(), request.scenes.end());
    for (const auto id : selected) {
        ViewFrameDiagnostic view_result{
            .view = id,
            .submitted = false,
            .surface_state = SurfaceState::Ready,
            .graph = {},
            .messages = {},
        };
        result.views.push_back(view_result);
        auto& diagnostic = result.views.back();
        if (!id.IsValid() || id.Index() >= impl_->view_slots.size()) {
            diagnostic.messages.emplace_back("view handle is stale");
            continue;
        }
        const auto& slot = impl_->view_slots[id.Index()];
        if (slot.generation != id.Generation() || !slot.descriptor) {
            diagnostic.messages.emplace_back("view handle is stale");
            continue;
        }
        const auto& descriptor = *slot.descriptor;
        if (!request.scenes.empty() && !selected_scenes.contains(descriptor.scene))
            continue;
        if (request.scenes.empty())
            selected_scenes.insert(descriptor.scene);

        FrameOutputBinding output;
        output.view = id;
        if (const auto* surface_output = std::get_if<SurfaceOutput>(&descriptor.output)) {
            const auto handle = surface_output->surface;
            if (!handle.IsValid() || handle.Index() >= impl_->surface_slots.size()) {
                diagnostic.surface_state = SurfaceState::Lost;
                diagnostic.messages.emplace_back("surface handle is stale");
                continue;
            }
            auto& surface = impl_->surface_slots[handle.Index()];
            if (surface.generation != handle.Generation() || surface.surface == nullptr) {
                diagnostic.surface_state = SurfaceState::Lost;
                diagnostic.messages.emplace_back("surface handle is stale");
                continue;
            }
            diagnostic.surface_state = surface.state;
            if (surface.state != SurfaceState::Ready)
                continue;
            if (!surface.acquired) {
                auto acquired = surface.swapchain->AcquireNextFrame();
                if (!acquired) {
                    surface.state = SurfaceState::Outdated;
                    diagnostic.surface_state = surface.state;
                    diagnostic.messages.emplace_back(acquired.error().Message());
                    continue;
                }
                surface.acquired = std::move(*acquired);
            }
            acquired_surfaces.insert(handle.Index());
            output.view_texture = surface.acquired->ColorViewRef();
            output.format = surface.swapchain->ColorFormat();
            output.width = surface.acquired->Width();
            output.height = surface.acquired->Height();
            output.initial_state = ExternalState::Present;
            output.final_state = ExternalState::Present;
            output.present = true;
            output.surface = handle;
        } else {
            const OffscreenTargetHandle handle = std::visit(
                [](const auto& value) -> OffscreenTargetHandle {
                    using T = std::decay_t<decltype(value)>;
                    if constexpr (std::is_same_v<T, OffscreenOutput> || std::is_same_v<T, ReadbackOutput>)
                        return value.target;
                    return {};
                },
                descriptor.output
            );
            if (!handle.IsValid() || handle.Index() >= impl_->offscreen_slots.size()) {
                diagnostic.messages.emplace_back("offscreen target handle is stale");
                continue;
            }
            auto& target = impl_->offscreen_slots[handle.Index()];
            if (target.generation != handle.Generation() || !target.descriptor) {
                diagnostic.messages.emplace_back("offscreen target handle is stale");
                continue;
            }
            output.texture = target.texture;
            output.view_texture = target.view;
            output.offscreen = handle;
            output.format = target.texture->GetFormat();
            output.width = target.descriptor->width;
            output.height = target.descriptor->height;
            output.initial_state = ExternalState::ShaderRead;
            output.final_state = ExternalState::ShaderRead;
        }
        outputs.push_back(output);
        RenderView view;
        view.id = id;
        view.scene = descriptor.scene;
        view.history = slot.history;
        view.camera = descriptor.camera;
        view.viewport = descriptor.viewport;
        view.render_width = descriptor.render_width;
        view.render_height = descriptor.render_height;
        view.visibility_mask = descriptor.visibility_mask;
        view.layer_mask = descriptor.layer_mask;
        view.exposure = descriptor.exposure;
        view.pipeline = descriptor.pipeline;
        view.temporal = descriptor.temporal;
        view.flags = descriptor.flags;
        auto& family = families[descriptor.family];
        family.family_id = descriptor.family;
        family.views.push_back(std::move(view));
    }

    if (outputs.empty() && !request.canvas) {
        for (const u32 index : acquired_surfaces) {
            impl_->surface_slots[index].swapchain->Discard();
            impl_->surface_slots[index].acquired.reset();
        }
        auto core = impl_->Frame(
            {
                .scenes = {},
                .view_families = {},
                .outputs = {},
                .time = request.time,
                .delta_time = request.delta_time,
                .canvas = request.canvas ? &*request.canvas : nullptr,
                .canvas_target = canvas_target ? &*canvas_target : nullptr,
            }
        );
        if (!core)
            return Err(std::move(core).error());
        if (core->submission.IsValid())
            result.submissions.emplace_back(core->submission.Value());
        impl_->device->Tick();
        impl_->instance->ProcessEvents();
        return Ok(std::move(result));
    }
    std::vector<CoreFrameRequest::Scene> scenes;
    for (const auto handle : selected_scenes) {
        if (!handle.IsValid() || handle.Index() >= impl_->scene_slots.size())
            return Err(ErrorCode::InvalidArgument, "frame scene handle is stale");
        auto& slot = impl_->scene_slots[handle.Index()];
        if (slot.generation != handle.Generation() || slot.state == nullptr || !slot.state->live)
            return Err(ErrorCode::InvalidArgument, "frame scene handle is stale");
        scenes.push_back({handle, slot.state->scene});
    }
    for (const auto& retired : impl_->retired_scenes)
        scenes.push_back(retired);
    std::vector<RenderViewFamily> family_values;
    for (auto& [_, family] : families)
        family_values.push_back(std::move(family));
    std::set<std::tuple<bool, u32, u32>> composed_targets;
    for (const auto& family : family_values)
        for (const auto& view : family.views) {
            auto output = std::ranges::find(outputs, view.id, &FrameOutputBinding::view);
            if (output == outputs.end())
                continue;
            const auto handle = output->present ? output->surface : SurfaceHandle{};
            const auto key = output->present
                                 ? std::tuple{true, handle.Index(), handle.Generation()}
                                 : std::tuple{false, output->offscreen.Index(), output->offscreen.Generation()};
            output->clear = composed_targets.insert(key).second;
        }
    auto core = impl_->Frame(
        {.scenes = scenes,
            .view_families = family_values,
            .outputs = outputs,
            .time = request.time,
            .delta_time = request.delta_time,
            .canvas = request.canvas ? &*request.canvas : nullptr,
            .canvas_target = canvas_target ? &*canvas_target : nullptr}
    );
    if (!core) {
        for (const u32 index : acquired_surfaces) {
            impl_->surface_slots[index].swapchain->Discard();
            impl_->surface_slots[index].acquired.reset();
        }
        return Err(std::move(core).error());
    }
    if (core->submission.IsValid())
        result.submissions.emplace_back(core->submission.Value());
    for (const auto& retired : impl_->retired_scenes)
        impl_->scene_worlds.erase(retired.handle);
    impl_->retired_scenes.clear();
    for (auto& view : result.views)
        if (std::ranges::find(core->submitted_views, view.view) != core->submitted_views.end()) {
            view.submitted = true;
            if (const auto found = impl_->executables.find(view.view); found != impl_->executables.end()) {
                view.graph.deterministic_hash = found->second->executor->Graph().DeterministicHash();
                view.graph.transient_bytes = found->second->executor->Graph().EstimatedTransientPeakBytes();
                view.graph.pass_count = static_cast<u32>(found->second->executor->Graph().Passes().size());
            }
        }
    for (const auto& item : core->diagnostics) {
        auto found = std::ranges::find(result.views, item.view, &ViewFrameDiagnostic::view);
        if (found != result.views.end())
            found->messages.push_back(item.code + ": " + item.message);
    }

    for (const u32 index : acquired_surfaces) {
        auto& surface = impl_->surface_slots[index];
        auto presented = surface.swapchain->Present();
        surface.acquired.reset();
        if (!presented)
            surface.state = SurfaceState::Lost;
    }
    impl_->device->Tick();
    impl_->instance->ProcessEvents();
    impl_->diagnostics.runtime_.frame_number = impl_->frame_number;
    impl_->diagnostics.runtime_.visible_objects = core->stats.visible_objects;
    impl_->diagnostics.runtime_.draw_packets = core->stats.packets;
    impl_->diagnostics.runtime_.draw_calls = core->stats.draw_calls;
    impl_->diagnostics.runtime_.skipped_resources = core->stats.skipped_pending_resources;
    impl_->diagnostics.runtime_.fallback_textures = impl_->textures->Stats().fallback_resolves;
    if (core->submission.IsValid())
        ++impl_->diagnostics.runtime_.submitted_frames;
    const auto resource_diagnostics = impl_->resources->Diagnostics();
    impl_->diagnostics.runtime_.resources = {};
    for (const auto& pool : resource_diagnostics.pools) {
        impl_->diagnostics.runtime_.resources.resident_bytes += pool.allocator.allocated_bytes;
        impl_->diagnostics.runtime_.resources.pending_bytes += pool.pending_free_bytes;
    }
    impl_->diagnostics.runtime_.resources.deferred_release_count = resource_diagnostics.deferred_releases;
    result.stats = impl_->diagnostics.runtime_;
    return Ok(std::move(result));
}

} // namespace woki::gfx
