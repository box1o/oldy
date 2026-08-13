#include <algorithm>

#include <woki/gfx/advanced/render_world.hpp>

namespace woki::gfx {
namespace {

void AppendObject(RenderWorldSnapshot::Objects& objects, const SceneHandle scene, const RenderObjectId id, const u64 version, const RenderObjectData& value) {
    objects.scenes.push_back(scene);
    objects.ids.push_back(id);
    objects.versions.push_back(version);
    objects.transforms.push_back(value.transform);
    // New objects have no prior sample; zero velocity by using the spawn transform.
    objects.previous_transforms.push_back(value.transform);
    objects.bounds.push_back(value.bounds);
    objects.meshes.push_back(value.mesh);
    objects.materials.push_back(value.material);
    objects.palettes.push_back(value.palette);
    objects.visibility_masks.push_back(value.visibility_mask);
    objects.layers.push_back(value.layers);
    objects.flags.push_back(value.flags);
    objects.material_phases.push_back(value.material_phase);
    objects.lods.push_back(value.lod);
    objects.feature_payload_ids.push_back(value.feature_payload_ids);
}

template <typename T>
void SwapErase(std::vector<T>& values, const u32 index) {
    if (index + 1 != values.size())
        values[index] = std::move(values.back());
    values.pop_back();
}

void RemoveObject(RenderWorldSnapshot::Objects& objects, const u32 index) {
    SwapErase(objects.scenes, index);
    SwapErase(objects.ids, index);
    SwapErase(objects.versions, index);
    SwapErase(objects.transforms, index);
    SwapErase(objects.previous_transforms, index);
    SwapErase(objects.bounds, index);
    SwapErase(objects.meshes, index);
    SwapErase(objects.materials, index);
    SwapErase(objects.palettes, index);
    SwapErase(objects.visibility_masks, index);
    SwapErase(objects.layers, index);
    SwapErase(objects.flags, index);
    SwapErase(objects.material_phases, index);
    SwapErase(objects.lods, index);
    SwapErase(objects.feature_payload_ids, index);
}

void ApplyPatch(RenderWorldSnapshot::Objects& objects, const u32 index, const RenderObjectPatch& patch) {
    if (patch.transform)
        objects.transforms[index] = *patch.transform;
    if (patch.previous_transform)
        objects.previous_transforms[index] = *patch.previous_transform;
    if (patch.bounds)
        objects.bounds[index] = *patch.bounds;
    if (patch.mesh)
        objects.meshes[index] = *patch.mesh;
    if (patch.material)
        objects.materials[index] = *patch.material;
    if (patch.palette)
        objects.palettes[index] = *patch.palette;
    if (patch.visibility_mask)
        objects.visibility_masks[index] = *patch.visibility_mask;
    if (patch.layers)
        objects.layers[index] = *patch.layers;
    if (patch.flags)
        objects.flags[index] = *patch.flags;
    if (patch.material_phase)
        objects.material_phases[index] = *patch.material_phase;
    if (patch.lod)
        objects.lods[index] = *patch.lod;
    if (patch.feature_payload_ids)
        objects.feature_payload_ids[index] = *patch.feature_payload_ids;
}

void MergePatch(RenderObjectPatch& target, const RenderObjectPatch& source) {
#define WOKI_MERGE_RENDER_FIELD(name)                                                                                                                                                                                      \
    if (source.name)                                                                                                                                                                                                       \
    target.name = source.name
    WOKI_MERGE_RENDER_FIELD(transform);
    WOKI_MERGE_RENDER_FIELD(previous_transform);
    WOKI_MERGE_RENDER_FIELD(bounds);
    WOKI_MERGE_RENDER_FIELD(mesh);
    WOKI_MERGE_RENDER_FIELD(material);
    WOKI_MERGE_RENDER_FIELD(palette);
    WOKI_MERGE_RENDER_FIELD(visibility_mask);
    WOKI_MERGE_RENDER_FIELD(layers);
    WOKI_MERGE_RENDER_FIELD(flags);
    WOKI_MERGE_RENDER_FIELD(material_phase);
    WOKI_MERGE_RENDER_FIELD(lod);
    WOKI_MERGE_RENDER_FIELD(feature_payload_ids);
#undef WOKI_MERGE_RENDER_FIELD
}

void ApplyPatch(RenderObjectData& object, const RenderObjectPatch& patch) {
#define WOKI_APPLY_RENDER_FIELD(name)                                                                                                                                                                                      \
    if (patch.name)                                                                                                                                                                                                        \
    object.name = *patch.name
    WOKI_APPLY_RENDER_FIELD(transform);
    WOKI_APPLY_RENDER_FIELD(previous_transform);
    WOKI_APPLY_RENDER_FIELD(bounds);
    WOKI_APPLY_RENDER_FIELD(mesh);
    WOKI_APPLY_RENDER_FIELD(material);
    WOKI_APPLY_RENDER_FIELD(palette);
    WOKI_APPLY_RENDER_FIELD(visibility_mask);
    WOKI_APPLY_RENDER_FIELD(layers);
    WOKI_APPLY_RENDER_FIELD(flags);
    WOKI_APPLY_RENDER_FIELD(material_phase);
    WOKI_APPLY_RENDER_FIELD(lod);
    WOKI_APPLY_RENDER_FIELD(feature_payload_ids);
#undef WOKI_APPLY_RENDER_FIELD
}

std::vector<RenderChange> Coalesce(std::span<const RenderChange> changes) {
    std::vector<RenderChange> ordered(changes.begin(), changes.end());
    std::stable_sort(ordered.begin(), ordered.end(), [](const RenderChange& left, const RenderChange& right) { return left.sequence < right.sequence; });
    std::vector<std::optional<RenderChange>> merged;
    std::map<RenderObjectId, size_t> objects;
    std::map<RenderLightId, size_t> lights;
    std::map<ViewId, size_t> views;
    for (const auto& envelope : ordered) {
        if (const auto* incoming = std::get_if<RenderObjectChange>(&envelope.payload)) {
            if (!incoming->id.IsValid())
                continue;
            const auto found = objects.find(incoming->id);
            if (found == objects.end()) {
                objects.emplace(incoming->id, merged.size());
                merged.emplace_back(envelope);
                continue;
            }
            auto& prior_envelope = *merged[found->second];
            auto& prior = std::get<RenderObjectChange>(prior_envelope.payload);
            if (prior.kind == SceneChangeKind::Destroy)
                continue;
            if (incoming->kind == SceneChangeKind::Destroy) {
                if (prior.kind == SceneChangeKind::Create) {
                    merged[found->second].reset();
                    objects.erase(found);
                } else if (incoming->version >= prior.version) {
                    prior = *incoming;
                    prior_envelope.sequence = envelope.sequence;
                    prior_envelope.epoch = envelope.epoch;
                }
                continue;
            }
            if (incoming->kind != SceneChangeKind::Update || incoming->version <= prior.version)
                continue;
            if (prior.kind == SceneChangeKind::Create && prior.create)
                ApplyPatch(*prior.create, incoming->patch);
            else
                MergePatch(prior.patch, incoming->patch);
            prior.version = incoming->version;
            prior_envelope.sequence = envelope.sequence;
            prior_envelope.epoch = envelope.epoch;
            continue;
        }
        const auto merge_value = [&](auto* incoming, auto& index) {
            if (!incoming->id.IsValid())
                return;
            const auto found = index.find(incoming->id);
            if (found == index.end()) {
                index.emplace(incoming->id, merged.size());
                merged.emplace_back(envelope);
                return;
            }
            auto& prior_envelope = *merged[found->second];
            using Change = std::remove_cv_t<std::remove_pointer_t<decltype(incoming)>>;
            auto& prior = std::get<Change>(prior_envelope.payload);
            if (prior.kind == SceneChangeKind::Destroy)
                return;
            if (incoming->kind == SceneChangeKind::Destroy && prior.kind == SceneChangeKind::Create) {
                merged[found->second].reset();
                index.erase(found);
                return;
            }
            if (incoming->version <= prior.version)
                return;
            prior = *incoming;
            prior_envelope.sequence = envelope.sequence;
            prior_envelope.epoch = envelope.epoch;
        };
        if (const auto* incoming = std::get_if<RenderLightChange>(&envelope.payload))
            merge_value(incoming, lights);
        else
            merge_value(&std::get<RenderViewChange>(envelope.payload), views);
    }
    std::vector<RenderChange> result;
    for (auto& change : merged)
        if (change)
            result.push_back(std::move(*change));
    std::ranges::sort(result, {}, &RenderChange::sequence);
    return result;
}

} // namespace

std::optional<u32> RenderWorldSnapshot::Resolve(const RenderObjectId id) const noexcept {
    if (!id.IsValid() || id.Index() >= object_slots_.size())
        return std::nullopt;
    const auto& slot = object_slots_[id.Index()];
    if (!slot.live || slot.generation != id.Generation() || slot.dense >= objects_.ids.size())
        return std::nullopt;
    return slot.dense;
}

RenderWorldBuilder::RenderWorldBuilder()
    : current_(std::make_shared<const RenderWorldSnapshot>()) {}

RenderWorldBuilder::RenderWorldBuilder(const SceneHandle scene) {
    auto snapshot = std::make_shared<RenderWorldSnapshot>();
    snapshot->scene_ = scene;
    current_ = std::move(snapshot);
}

Result<std::shared_ptr<const RenderWorldSnapshot>> RenderWorldBuilder::Apply(const std::span<const RenderChange> changes) {
    auto next = std::make_shared<RenderWorldSnapshot>(*current_);
    ++next->generation_;
    auto ordered = Coalesce(changes);
    if (!ordered.empty()) {
        if (next->scene_.IsValid() && next->scene_ != ordered.front().scene)
            return Err(ErrorCode::ValidationInvalidState, "render-world changes mix scene identities");
        next->scene_ = ordered.front().scene;
    }

    for (const auto& envelope : ordered) {
        if (const auto* change = std::get_if<RenderObjectChange>(&envelope.payload)) {
            if (!change->id.IsValid())
                continue;
            if (change->id.Index() >= next->object_slots_.size())
                next->object_slots_.resize(static_cast<size_t>(change->id.Index()) + 1);
            auto& slot = next->object_slots_[change->id.Index()];
            if (change->kind == SceneChangeKind::Create) {
                if (!change->create || slot.live || change->id.Generation() <= slot.generation)
                    continue;
                slot = {change->id.Generation(), static_cast<u32>(next->objects_.ids.size()), true};
                AppendObject(next->objects_, envelope.scene, change->id, change->version, *change->create);
                continue;
            }
            const auto dense = next->Resolve(change->id);
            if (!dense || (change->kind == SceneChangeKind::Destroy ? change->version < next->objects_.versions[*dense] : change->version <= next->objects_.versions[*dense]))
                continue;
            if (change->kind == SceneChangeKind::Destroy) {
                const u32 removed = *dense;
                const u32 last = static_cast<u32>(next->objects_.ids.size() - 1);
                slot.live = false;
                slot.generation = change->id.Generation();
                if (removed != last)
                    next->object_slots_[next->objects_.ids[last].Index()].dense = removed;
                RemoveObject(next->objects_, removed);
            } else {
                ApplyPatch(next->objects_, *dense, change->patch);
                next->objects_.versions[*dense] = change->version;
            }
            continue;
        }

        if (const auto* change = std::get_if<RenderLightChange>(&envelope.payload)) {
            if (!change->id.IsValid())
                continue;
            if (change->id.Index() >= next->light_generations_.size())
                next->light_generations_.resize(static_cast<size_t>(change->id.Index()) + 1);
            const auto found = std::ranges::find(next->light_ids_, change->id);
            const size_t index = static_cast<size_t>(found - next->light_ids_.begin());
            if (change->kind == SceneChangeKind::Create) {
                if (change->value && found == next->light_ids_.end() && change->id.Generation() > next->light_generations_[change->id.Index()]) {
                    next->light_generations_[change->id.Index()] = change->id.Generation();
                    next->light_ids_.push_back(change->id);
                    next->light_scenes_.push_back(envelope.scene);
                    next->light_versions_.push_back(change->version);
                    next->lights_.push_back(*change->value);
                }
            } else if (found != next->light_ids_.end() && (change->kind == SceneChangeKind::Destroy ? change->version >= next->light_versions_[index] : change->version > next->light_versions_[index])) {
                if (change->kind == SceneChangeKind::Destroy) {
                    next->light_generations_[change->id.Index()] = change->id.Generation();
                    SwapErase(next->light_ids_, static_cast<u32>(index));
                    SwapErase(next->light_scenes_, static_cast<u32>(index));
                    SwapErase(next->light_versions_, static_cast<u32>(index));
                    SwapErase(next->lights_, static_cast<u32>(index));
                } else if (change->value) {
                    next->light_versions_[index] = change->version;
                    next->lights_[index] = *change->value;
                }
            }
            continue;
        }

        const auto& change = std::get<RenderViewChange>(envelope.payload);
        if (!change.id.IsValid())
            continue;
        if (change.id.Index() >= next->view_generations_.size())
            next->view_generations_.resize(static_cast<size_t>(change.id.Index()) + 1);
        const auto found = std::ranges::find(next->view_ids_, change.id);
        const size_t index = static_cast<size_t>(found - next->view_ids_.begin());
        if (change.kind == SceneChangeKind::Create) {
            if (change.value && found == next->view_ids_.end() && change.id.Generation() > next->view_generations_[change.id.Index()]) {
                next->view_generations_[change.id.Index()] = change.id.Generation();
                next->view_ids_.push_back(change.id);
                next->view_versions_.push_back(change.version);
                next->views_.push_back(*change.value);
            }
        } else if (found != next->view_ids_.end() && (change.kind == SceneChangeKind::Destroy ? change.version >= next->view_versions_[index] : change.version > next->view_versions_[index])) {
            if (change.kind == SceneChangeKind::Destroy) {
                next->view_generations_[change.id.Index()] = change.id.Generation();
                SwapErase(next->view_ids_, static_cast<u32>(index));
                SwapErase(next->view_versions_, static_cast<u32>(index));
                SwapErase(next->views_, static_cast<u32>(index));
            } else if (change.value) {
                next->view_versions_[index] = change.version;
                next->views_[index] = *change.value;
            }
        }
    }

    current_ = next;
    return Ok(std::shared_ptr<const RenderWorldSnapshot>(std::move(next)));
}

} // namespace woki::gfx
