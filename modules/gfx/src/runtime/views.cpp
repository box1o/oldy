#include "../internal/runtime_state.hpp"

namespace woki::gfx {

Result<ViewId> RenderRuntime::CreateView(ViewDescriptor descriptor) {
    if (!descriptor.scene.IsValid() || descriptor.scene.Index() >= impl_->scene_slots.size() || impl_->scene_slots[descriptor.scene.Index()].generation != descriptor.scene.Generation())
        return Err(ErrorCode::InvalidArgument, "view requires a live scene");
    u32 index{};
    if (impl_->free_views.empty()) {
        index = static_cast<u32>(impl_->view_slots.size());
        impl_->view_slots.emplace_back();
    } else {
        index = impl_->free_views.back();
        impl_->free_views.pop_back();
    }
    auto& slot = impl_->view_slots[index];
    slot.descriptor = std::move(descriptor);
    slot.history = ViewHistoryId::Create(index, slot.generation);
    slot.version = 1;
    return Ok(ViewId::Create(index, slot.generation));
}

Result<void> RenderRuntime::UpdateView(const ViewId view, ViewDescriptor descriptor) {
    if (!view.IsValid() || view.Index() >= impl_->view_slots.size())
        return Err(ErrorCode::InvalidArgument, "view handle is stale");
    auto& slot = impl_->view_slots[view.Index()];
    if (slot.generation != view.Generation() || !slot.descriptor)
        return Err(ErrorCode::InvalidArgument, "view handle is stale");
    if (!descriptor.scene.IsValid() || descriptor.scene.Index() >= impl_->scene_slots.size())
        return Err(ErrorCode::InvalidArgument, "view requires a live scene");
    const auto& scene = impl_->scene_slots[descriptor.scene.Index()];
    if (scene.generation != descriptor.scene.Generation() || scene.state == nullptr || !scene.state->live)
        return Err(ErrorCode::InvalidArgument, "view requires a live scene");
    slot.descriptor = std::move(descriptor);
    ++slot.version;
    return Ok();
}

Result<void> RenderRuntime::DestroyView(const ViewId view) {
    if (!view.IsValid() || view.Index() >= impl_->view_slots.size())
        return Err(ErrorCode::InvalidArgument, "view handle is stale");
    auto& slot = impl_->view_slots[view.Index()];
    if (slot.generation != view.Generation() || !slot.descriptor)
        return Err(ErrorCode::InvalidArgument, "view handle is stale");
    impl_->executables.erase(view);
    slot.descriptor.reset();
    ++slot.generation;
    if (slot.generation == 0)
        ++slot.generation;
    impl_->free_views.push_back(view.Index());
    return Ok();
}

Result<PipelineHandle> RenderRuntime::CreatePipeline() {
    return Ok(impl_->pipelines.Create());
}

Result<void> RenderRuntime::PublishPipeline(const PipelineHandle pipeline, const asset::Product& product) {
    return impl_->pipelines.Publish(pipeline, product);
}

Result<MeshHandle> RenderRuntime::RequestMesh(const asset::AssetId id) {
    return impl_->meshes->Request(id);
}

MeshState RenderRuntime::MeshStatus(const MeshHandle handle) const noexcept {
    return impl_->meshes->State(handle);
}

Result<MaterialInstanceHandle> RenderRuntime::RequestMaterial(const asset::AssetId id) {
    return impl_->materials->RequestInstance(id);
}

SkinPaletteHandle RenderRuntime::CreateSkinPalette(const u32 joint_count) {
    return impl_->palettes->Create(joint_count);
}

Result<void> RenderRuntime::UpdateSkinPalette(const SkinPaletteHandle handle, const std::span<const math::mat4f> matrices, const u64 source_generation) {
    TRY_VOID(impl_->palettes->Set(handle, matrices, source_generation));
    return impl_->palettes->Upload(handle);
}

Result<AnimationPlaybackInfo> RenderRuntime::CreateAnimationPlayback(const MeshHandle mesh) {
    const auto* product = impl_->meshes->Product(mesh);
    if (product == nullptr)
        return Err(ErrorCode::InvalidState, "mesh animation data is not resident yet");

    SkeletonRegistry skeletons;
    AnimationClipRegistry clips;
    const auto skeleton_handles = skeletons.RegisterAll(*product, 1);
    const auto clip_handles = clips.Register(*product, 1);
    if (skeleton_handles.empty() || clip_handles.empty())
        return Err(ErrorCode::InvalidState, "mesh product has no usable animation");

    const auto clip = clips.Get(clip_handles.front());
    if (clip == nullptr)
        return Err(ErrorCode::ValidationInvalidState, "animation clip handle did not resolve");
    const u32 skeleton_index = std::min<u32>(clip->skeleton, static_cast<u32>(skeleton_handles.size() - 1));
    const auto skeleton = skeletons.Get(skeleton_handles[skeleton_index]);
    if (skeleton == nullptr || skeleton->joints.empty())
        return Err(ErrorCode::InvalidState, "animation skeleton has no joints");

    // Assimp import bakes each mesh node's world placement into the cooked
    // vertex stream.  The runtime object therefore must not apply the node
    // transform a second time.
    const math::mat4f mesh_transform = math::mat4f::identity();
    auto animator = createScope<Animator>(skeleton, mesh_transform);
    std::vector<ref<const AnimationClip>> animation_clips;
    animation_clips.reserve(clip_handles.size());
    for (const auto handle : clip_handles)
        animation_clips.push_back(clips.Get(handle));
    animator->SetClips(std::move(animation_clips));
    TRY_VOID(animator->Select(0, true));

    const u32 index = static_cast<u32>(impl_->animation_slots.size());
    auto& slot = impl_->animation_slots.emplace_back();
    slot.animator = std::move(animator);
    slot.palette = CreateSkinPalette(static_cast<u32>(skeleton->joints.size()));
    const auto playback = AnimationPlaybackHandle::Create(index, slot.generation);

    AnimationPlaybackInfo info;
    info.playback = playback;
    info.skeleton = skeleton_handles[skeleton_index];
    info.clip = clip_handles.front();
    info.palette = slot.palette;
    info.bounds.minimum = product->bounds_min;
    info.bounds.maximum = product->bounds_max;
    info.bounds.center = (info.bounds.minimum + info.bounds.maximum) * 0.5F;
    const auto extent = info.bounds.maximum - info.bounds.center;
    info.bounds.radius = std::sqrt(extent.x * extent.x + extent.y * extent.y + extent.z * extent.z);
    info.model_transform = mesh_transform;
    info.clip_name = clip->name;
    info.duration = clip->duration;
    info.joint_count = static_cast<u32>(skeleton->joints.size());
    info.mesh_count = product->mesh_count;
    info.lod_count = static_cast<u32>(product->lods.size());
    return Ok(std::move(info));
}

Result<void> RenderRuntime::AdvanceAnimation(const AnimationPlaybackHandle playback, const f32 delta_seconds) {
    if (!playback.IsValid() || playback.Index() >= impl_->animation_slots.size())
        return Err(ErrorCode::InvalidArgument, "animation playback handle is stale");
    auto& slot = impl_->animation_slots[playback.Index()];
    if (slot.generation != playback.Generation() || slot.animator == nullptr)
        return Err(ErrorCode::InvalidArgument, "animation playback handle is stale");
    slot.animator->Update(delta_seconds);
    auto pose = slot.animator->Evaluate();
    if (!pose)
        return Err(std::move(pose).error());
    return UpdateSkinPalette(slot.palette, *pose, ++slot.pose_generation);
}

Result<void> RenderRuntime::SetAnimationPaused(const AnimationPlaybackHandle playback, const bool paused) {
    if (!playback.IsValid() || playback.Index() >= impl_->animation_slots.size() || impl_->animation_slots[playback.Index()].generation != playback.Generation()
        || impl_->animation_slots[playback.Index()].animator == nullptr)
        return Err(ErrorCode::InvalidArgument, "animation playback handle is stale");
    impl_->animation_slots[playback.Index()].animator->State().paused = paused;
    return Ok();
}

Result<void> RenderRuntime::RestartAnimation(const AnimationPlaybackHandle playback) {
    if (!playback.IsValid() || playback.Index() >= impl_->animation_slots.size() || impl_->animation_slots[playback.Index()].generation != playback.Generation()
        || impl_->animation_slots[playback.Index()].animator == nullptr)
        return Err(ErrorCode::InvalidArgument, "animation playback handle is stale");
    impl_->animation_slots[playback.Index()].animator->State().time = 0.0F;
    return Ok();
}

Result<void> RenderRuntime::ChangeAnimationSpeed(const AnimationPlaybackHandle playback, const f32 amount) {
    if (!playback.IsValid() || playback.Index() >= impl_->animation_slots.size() || impl_->animation_slots[playback.Index()].generation != playback.Generation()
        || impl_->animation_slots[playback.Index()].animator == nullptr)
        return Err(ErrorCode::InvalidArgument, "animation playback handle is stale");
    auto& speed = impl_->animation_slots[playback.Index()].animator->State().speed;
    speed = std::clamp(speed + amount, 0.0F, 4.0F);
    return Ok();
}

} // namespace woki::gfx
