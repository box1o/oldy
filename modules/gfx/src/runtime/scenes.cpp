#include "../internal/runtime_state.hpp"

namespace woki::gfx {

struct SceneMutation::Impl final {
    ref<SceneRuntimeState> state;
    SceneHandle handle;
    RenderChangeJournal::Writer writer;
    std::unordered_map<RenderObjectId, u64> initial_object_versions;
    std::unordered_map<RenderLightId, u64> initial_light_versions;
    std::vector<RenderObjectId> created_objects;
    std::vector<RenderLightId> created_lights;
    std::vector<RenderObjectId> destroyed_objects;
    std::vector<RenderLightId> destroyed_lights;
    bool finalized{};
};

SceneMutation::SceneMutation() = default;

SceneMutation::SceneMutation(scope<Impl> impl)
    : impl_(std::move(impl)) {}

SceneMutation::SceneMutation(SceneMutation&&) noexcept = default;

SceneMutation& SceneMutation::operator=(SceneMutation&& other) noexcept {
    if (this != &other) {
        if (impl_ != nullptr && !impl_->finalized) {
            std::fputs("woki::gfx: overwritten SceneMutation was abandoned\n", stderr);
            Cancel();
        }
        impl_ = std::move(other.impl_);
    }
    return *this;
}

SceneMutation::~SceneMutation() {
    if (impl_ != nullptr && !impl_->finalized) {
        std::fputs("woki::gfx: abandoned SceneMutation; call Commit() or Cancel() explicitly\n", stderr);
        Cancel();
    }
}

Result<RenderObjectId> SceneMutation::CreateObject(RenderObjectData data) {
    if (impl_ == nullptr || impl_->finalized)
        return Err(ErrorCode::InvalidState, "scene mutation is finalized");
    std::lock_guard lock(impl_->state->mutex);
    if (!impl_->state->live || impl_->state->handle != impl_->handle)
        return Err(ErrorCode::InvalidState, "scene was destroyed or reused");
    RenderObjectId id;
    TRY_ASSIGN(id, impl_->state->scene->CreateObject(impl_->writer, std::move(data)));
    impl_->state->object_versions[id] = 1;
    impl_->created_objects.push_back(id);
    return Ok(id);
}

Result<void> SceneMutation::UpdateObject(const RenderObjectId id, RenderObjectPatch patch) {
    if (impl_ == nullptr || impl_->finalized)
        return Err(ErrorCode::InvalidState, "scene mutation is finalized");
    std::lock_guard lock(impl_->state->mutex);
    if (!impl_->state->live || impl_->state->handle != impl_->handle)
        return Err(ErrorCode::InvalidState, "scene was destroyed or reused");
    auto found = impl_->state->object_versions.find(id);
    if (found == impl_->state->object_versions.end())
        return Err(ErrorCode::InvalidArgument, "object does not belong to this scene");
    return impl_->state->scene->UpdateObject(impl_->writer, id, ++found->second, std::move(patch));
}

Result<void> SceneMutation::DestroyObject(const RenderObjectId id) {
    if (impl_ == nullptr || impl_->finalized)
        return Err(ErrorCode::InvalidState, "scene mutation is finalized");
    std::lock_guard lock(impl_->state->mutex);
    if (!impl_->state->live || impl_->state->handle != impl_->handle)
        return Err(ErrorCode::InvalidState, "scene was destroyed or reused");
    auto found = impl_->state->object_versions.find(id);
    if (found == impl_->state->object_versions.end())
        return Err(ErrorCode::InvalidArgument, "object does not belong to this scene");
    TRY_VOID(impl_->state->scene->QueueDestroyObject(impl_->writer, id, ++found->second));
    impl_->destroyed_objects.push_back(id);
    impl_->state->object_versions.erase(found);
    return Ok();
}

Result<RenderLightId> SceneMutation::CreateLight(RenderLightData data) {
    if (impl_ == nullptr || impl_->finalized)
        return Err(ErrorCode::InvalidState, "scene mutation is finalized");
    std::lock_guard lock(impl_->state->mutex);
    if (!impl_->state->live || impl_->state->handle != impl_->handle)
        return Err(ErrorCode::InvalidState, "scene was destroyed or reused");
    RenderLightId id;
    TRY_ASSIGN(id, impl_->state->scene->CreateLight(impl_->writer, std::move(data)));
    impl_->state->light_versions[id] = 1;
    impl_->created_lights.push_back(id);
    return Ok(id);
}

Result<void> SceneMutation::UpdateLight(const RenderLightId id, RenderLightData data) {
    if (impl_ == nullptr || impl_->finalized)
        return Err(ErrorCode::InvalidState, "scene mutation is finalized");
    std::lock_guard lock(impl_->state->mutex);
    if (!impl_->state->live || impl_->state->handle != impl_->handle)
        return Err(ErrorCode::InvalidState, "scene was destroyed or reused");
    auto found = impl_->state->light_versions.find(id);
    if (found == impl_->state->light_versions.end())
        return Err(ErrorCode::InvalidArgument, "light does not belong to this scene");
    return impl_->state->scene->UpdateLight(impl_->writer, id, ++found->second, std::move(data));
}

Result<void> SceneMutation::DestroyLight(const RenderLightId id) {
    if (impl_ == nullptr || impl_->finalized)
        return Err(ErrorCode::InvalidState, "scene mutation is finalized");
    std::lock_guard lock(impl_->state->mutex);
    if (!impl_->state->live || impl_->state->handle != impl_->handle)
        return Err(ErrorCode::InvalidState, "scene was destroyed or reused");
    auto found = impl_->state->light_versions.find(id);
    if (found == impl_->state->light_versions.end())
        return Err(ErrorCode::InvalidArgument, "light does not belong to this scene");
    TRY_VOID(impl_->state->scene->QueueDestroyLight(impl_->writer, id, ++found->second));
    impl_->destroyed_lights.push_back(id);
    impl_->state->light_versions.erase(found);
    return Ok();
}

Result<void> SceneMutation::Commit() {
    if (impl_ == nullptr || impl_->finalized)
        return Ok();
    std::lock_guard lock(impl_->state->mutex);
    if (!impl_->state->live || impl_->state->handle != impl_->handle)
        return Err(ErrorCode::InvalidState, "scene was destroyed or reused before commit");
    TRY_VOID(impl_->writer.Flush());
    for (const auto id : impl_->destroyed_objects)
        TRY_VOID(impl_->state->scene->ReleaseObject(id));
    for (const auto id : impl_->destroyed_lights)
        TRY_VOID(impl_->state->scene->ReleaseLight(id));
    impl_->finalized = true;
    return Ok();
}

void SceneMutation::Cancel() noexcept {
    if (impl_ == nullptr || impl_->finalized)
        return;
    impl_->writer.Cancel();
    try {
        std::lock_guard lock(impl_->state->mutex);
        if (!impl_->state->live || impl_->state->handle != impl_->handle) {
            impl_->finalized = true;
            return;
        }
        for (const auto id : impl_->created_objects)
            static_cast<void>(impl_->state->scene->ReleaseObject(id));
        for (const auto id : impl_->created_lights)
            static_cast<void>(impl_->state->scene->ReleaseLight(id));
        impl_->state->object_versions.swap(impl_->initial_object_versions);
        impl_->state->light_versions.swap(impl_->initial_light_versions);
    } catch (...) {
        std::fputs("woki::gfx: SceneMutation cancellation could not fully restore local bookkeeping\n", stderr);
    }
    impl_->finalized = true;
}

Result<SceneHandle> RenderRuntime::CreateScene(SceneDescriptor descriptor) {
    u32 index{};
    if (impl_->free_scenes.empty()) {
        index = static_cast<u32>(impl_->scene_slots.size());
        if (index >= 4095)
            return Err(ErrorCode::InvalidState, "scene registry exhausted its logical ID namespace");
        impl_->scene_slots.emplace_back();
    } else {
        index = impl_->free_scenes.back();
        impl_->free_scenes.pop_back();
    }
    auto& slot = impl_->scene_slots[index];
    const auto handle = SceneHandle::Create(index, slot.generation);
    slot.state = createRef<SceneRuntimeState>();
    slot.state->handle = handle;
    slot.state->scene = createRef<RenderScene>(handle, index + 1);
    slot.label = std::move(descriptor.label);
    return Ok(handle);
}

Result<void> RenderRuntime::DestroyScene(const SceneHandle scene) {
    if (!scene.IsValid() || scene.Index() >= impl_->scene_slots.size())
        return Err(ErrorCode::InvalidArgument, "scene handle is stale");
    auto& slot = impl_->scene_slots[scene.Index()];
    if (slot.generation != scene.Generation() || slot.state == nullptr || !slot.state->live)
        return Err(ErrorCode::InvalidArgument, "scene handle is stale");
    for (const auto& view : impl_->view_slots)
        if (view.descriptor && view.descriptor->scene == scene)
            return Err(ErrorCode::InvalidState, "destroy all scene views before destroying the scene");
    std::lock_guard state_lock(slot.state->mutex);
    auto mutation = slot.state->scene->CreateWriter();
    for (const auto& [id, version] : slot.state->object_versions)
        TRY_VOID(slot.state->scene->DestroyObject(mutation, id, version + 1));
    for (const auto& [id, version] : slot.state->light_versions)
        TRY_VOID(slot.state->scene->DestroyLight(mutation, id, version + 1));
    TRY_VOID(mutation.Flush());
    impl_->retired_scenes.push_back({scene, slot.state->scene});
    slot.state->object_versions.clear();
    slot.state->light_versions.clear();
    slot.state->live = false;
    slot.state.reset();
    ++slot.generation;
    if (slot.generation == 0)
        ++slot.generation;
    impl_->free_scenes.push_back(scene.Index());
    return Ok();
}

Result<SceneMutation> RenderRuntime::MutateScene(const SceneHandle scene) {
    if (!scene.IsValid() || scene.Index() >= impl_->scene_slots.size())
        return Err(ErrorCode::InvalidArgument, "scene handle is stale");
    auto& slot = impl_->scene_slots[scene.Index()];
    if (slot.generation != scene.Generation() || slot.state == nullptr || !slot.state->live)
        return Err(ErrorCode::InvalidArgument, "scene handle is stale");
    auto mutation = createScope<SceneMutation::Impl>();
    mutation->state = slot.state;
    mutation->handle = scene;
    mutation->writer = slot.state->scene->CreateWriter();
    mutation->initial_object_versions = slot.state->object_versions;
    mutation->initial_light_versions = slot.state->light_versions;
    return Ok(SceneMutation(std::move(mutation)));
}

} // namespace woki::gfx
