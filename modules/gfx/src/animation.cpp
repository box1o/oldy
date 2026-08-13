#include <algorithm>
#include <cmath>
#include <cstring>
#include <map>
#include <numeric>
#include <string_view>

#include <woki/gfx/advanced/animation.hpp>

namespace woki::gfx {
namespace {
struct Trs {
    math::vec3f translation{};
    math::quatf rotation{};
    math::vec3f scale{1.0F};
};

Trs Decompose(const math::mat4f& matrix) {
    Trs result;
    result.translation = {matrix(0, 3), matrix(1, 3), matrix(2, 3)};
    result.scale = {std::sqrt(matrix(0, 0) * matrix(0, 0) + matrix(1, 0) * matrix(1, 0) + matrix(2, 0) * matrix(2, 0)),
        std::sqrt(matrix(0, 1) * matrix(0, 1) + matrix(1, 1) * matrix(1, 1) + matrix(2, 1) * matrix(2, 1)),
        std::sqrt(matrix(0, 2) * matrix(0, 2) + matrix(1, 2) * matrix(1, 2) + matrix(2, 2) * matrix(2, 2))};
    for (u32 axis = 0; axis < 3; ++axis)
        if (result.scale[axis] <= 1.0e-8F)
            result.scale[axis] = 1.0F;
    math::mat4f rotation = matrix;
    for (u32 column = 0; column < 3; ++column)
        for (u32 row = 0; row < 3; ++row)
            rotation(row, column) /= result.scale[column];
    rotation(0, 3) = rotation(1, 3) = rotation(2, 3) = 0.0F;
    rotation(3, 0) = rotation(3, 1) = rotation(3, 2) = 0.0F;
    rotation(3, 3) = 1.0F;
    result.rotation = math::quatf::fromMat4(rotation);
    return result;
}

math::mat4f Compose(const Trs& value) {
    return math::translate(value.translation) * value.rotation.toMat4() * math::scale(value.scale);
}

math::vec4f Sample(const MeshAnimationChannel& channel, const f32 time) {
    if (channel.times.empty())
        return {};
    const auto upper = std::ranges::upper_bound(channel.times, time);
    if (upper == channel.times.begin())
        return channel.values.front();
    if (upper == channel.times.end())
        return channel.values.back();
    const size_t right = static_cast<size_t>(upper - channel.times.begin()), left = right - 1;
    if (channel.interpolation == AnimationInterpolation::Step)
        return channel.values[left];
    const f32 factor = (time - channel.times[left]) / (channel.times[right] - channel.times[left]);
    if (channel.path == AnimationPath::Rotation) {
        const math::quatf a{channel.values[left].x,
            channel.values[left].y,
            channel.values[left].z,
            channel.values[left].w};
        const math::quatf b{channel.values[right].x,
            channel.values[right].y,
            channel.values[right].z,
            channel.values[right].w};
        const auto result = math::slerp(a, b, factor);
        return {result.x, result.y, result.z, result.w};
    }
    return channel.values[left] + (channel.values[right] - channel.values[left]) * factor;
}
} // namespace

SkeletonHandle SkeletonRegistry::Register(const MeshProduct& product, const u64 generation) {
    const u32 index = static_cast<u32>(slots_.size());
    slots_.push_back(
        {1,
            createRef<const SkeletonAsset>(SkeletonAsset{
                generation,
                product.skeleton,
                std::vector<math::mat4f>(product.skeleton.size(), math::mat4f::identity()),
                {},
                {},
                {},
                {},
            })}
    );
    return SkeletonHandle::Create(index, 1);
}

std::vector<SkeletonHandle> SkeletonRegistry::RegisterAll(const MeshProduct& product, const u64 generation) {
    if (product.source_skeletons.empty() || product.skeleton.empty())
        return {Register(product, generation)};

    const auto& source = product.source_skeletons.front();
    std::map<std::string_view, u32, std::less<>> source_by_name;
    for (u32 index = 0; index < source.joints.size(); ++index)
        source_by_name.emplace(source.joints[index].name, index);

    std::vector<math::mat4f> prefixes;
    std::vector<u32> joint_nodes;
    prefixes.reserve(product.skeleton.size());
    joint_nodes.reserve(product.skeleton.size());
    for (const auto& joint : product.skeleton) {
        const auto source_joint = source_by_name.find(joint.name);
        if (source_joint == source_by_name.end()) {
            prefixes.push_back(math::mat4f::identity());
            joint_nodes.push_back(0);
            continue;
        }
        const u32 source_index = source_joint->second;
        const auto& node = product.nodes[source.joints[source_index].node];
        prefixes.push_back(joint.bind_local * node.local.inverse());
        joint_nodes.push_back(source.joints[source_index].node);
    }

    const u32 index = static_cast<u32>(slots_.size());
    slots_.push_back(
        {1,
            createRef<const SkeletonAsset>(SkeletonAsset{
                generation,
                product.skeleton,
                std::move(prefixes),
                std::move(joint_nodes),
                [&] {
                    std::vector<i32> parents;
                    parents.reserve(product.nodes.size());
                    for (const auto& node : product.nodes)
                        parents.push_back(node.parent);
                    return parents;
                }(),
                [&] {
                    std::vector<math::mat4f> locals;
                    locals.reserve(product.nodes.size());
                    for (const auto& node : product.nodes)
                        locals.push_back(node.local);
                    return locals;
                }(),
                [&] {
                    std::vector<math::mat4f> prefixes;
                    prefixes.reserve(product.nodes.size());
                    for (const auto& node : product.nodes) {
                        const auto trs = Decompose(node.local);
                        prefixes.push_back(node.local * Compose(trs).inverse());
                    }
                    return prefixes;
                }(),
            })}
    );
    return {SkeletonHandle::Create(index, 1)};
}

ref<const SkeletonAsset> SkeletonRegistry::Get(const SkeletonHandle handle) const noexcept {
    return handle.IsValid() && handle.Index() < slots_.size()
                   && slots_[handle.Index()].generation == handle.Generation()
               ? slots_[handle.Index()].value
               : nullptr;
}

std::vector<AnimationClipHandle> AnimationClipRegistry::Register(const MeshProduct& product, const u64 generation) {
    std::vector<AnimationClipHandle> result;
    result.reserve(product.animations.size());
    for (const auto& clip : product.animations) {
        const u32 index = static_cast<u32>(slots_.size());
        slots_.push_back(
            {1,
                createRef<const AnimationClip>(
                    AnimationClip{generation, clip.name, clip.skeleton, clip.duration, clip.channels}
                )}
        );
        result.push_back(AnimationClipHandle::Create(index, 1));
    }
    return result;
}

ref<const AnimationClip> AnimationClipRegistry::Get(const AnimationClipHandle handle) const noexcept {
    return handle.IsValid() && handle.Index() < slots_.size()
                   && slots_[handle.Index()].generation == handle.Generation()
               ? slots_[handle.Index()].value
               : nullptr;
}

Animator::Animator(ref<const SkeletonAsset> skeleton, const math::mat4f mesh_transform)
    : skeleton_(std::move(skeleton)),
      inverse_mesh_transform_(mesh_transform.inverse()) {}

void Animator::SetClips(std::vector<ref<const AnimationClip>> clips) {
    clips_ = std::move(clips);
    if (state_.clip >= clips_.size())
        state_ = {};
}

Result<void> Animator::Select(const u32 clip, const bool loop) {
    if (clip >= clips_.size())
        return Err(ErrorCode::ValidationOutOfRange, "animation clip is out of range");
    state_.clip = clip;
    state_.time = 0.0F;
    state_.loop = loop;
    return Ok();
}

void Animator::Update(const f32 delta_seconds) {
    if (state_.paused || state_.clip >= clips_.size())
        return;
    const f32 duration = clips_[state_.clip]->duration;
    state_.time += delta_seconds * state_.speed;
    if (duration <= 0.0F)
        state_.time = 0.0F;
    else if (state_.loop) {
        state_.time = std::fmod(state_.time, duration);
        if (state_.time < 0.0F)
            state_.time += duration;
    } else
        state_.time = std::clamp(state_.time, 0.0F, duration);
}

Result<std::vector<math::mat4f>> Animator::Evaluate() const {
    if (skeleton_ == nullptr)
        return Err(ErrorCode::InvalidState, "animator has no skeleton");
    if (skeleton_->node_locals.empty() || skeleton_->joint_nodes.size() != skeleton_->joints.size())
        return Ok(std::vector<math::mat4f>(skeleton_->joints.size(), math::mat4f::identity()));
    std::vector<Trs> local;
    local.reserve(skeleton_->node_locals.size());
    for (const auto& matrix : skeleton_->node_locals)
        local.push_back(Decompose(matrix));
    if (state_.clip < clips_.size())
        for (const auto& channel : clips_[state_.clip]->channels) {
            if (channel.joint >= skeleton_->joint_nodes.size())
                return Err(ErrorCode::ValidationOutOfRange, "animation channel joint is out of range");
            const auto value = Sample(channel, state_.time);
            const u32 node = skeleton_->joint_nodes[channel.joint];
            if (node >= local.size())
                return Err(ErrorCode::ValidationOutOfRange, "animation joint node is out of range");
            auto& target = local[node];
            if (channel.path == AnimationPath::Translation)
                target.translation = {value.x, value.y, value.z};
            else if (channel.path == AnimationPath::Scale)
                target.scale = {value.x, value.y, value.z};
            else
                target.rotation = math::quatf{value.x, value.y, value.z, value.w}.normalized();
        }
    std::vector<math::mat4f> bind_globals(local.size()), globals(local.size());
    for (u32 index = 0; index < local.size(); ++index) {
        const i32 parent = index < skeleton_->node_parents.size() ? skeleton_->node_parents[index] : -1;
        const auto bind = skeleton_->node_locals[index];
        const math::mat4f prefix = index < skeleton_->node_prefixes.size()
                                       ? skeleton_->node_prefixes[index]
                                       : math::mat4f::identity();
        const auto animated = prefix * Compose(local[index]);
        bind_globals[index] = parent < 0 ? bind : bind_globals[static_cast<u32>(parent)] * bind;
        globals[index] = parent < 0 ? animated : globals[static_cast<u32>(parent)] * animated;
    }
    std::vector<math::mat4f> skin(skeleton_->joints.size());
    for (u32 index = 0; index < skin.size(); ++index) {
        const u32 node = skeleton_->joint_nodes[index];
        skin[index] = globals[node] * bind_globals[node].inverse();
    }
    return Ok(std::move(skin));
}

SkinPalette::SkinPalette(const u32 joint_count)
    : matrices_(joint_count, math::mat4f::identity()),
      previous_matrices_(joint_count, math::mat4f::identity()),
      dirty_(static_cast<u64>(joint_count) * sizeof(math::mat4f)) {
    dirty_.Mark(0, static_cast<u64>(joint_count) * sizeof(math::mat4f));
}

Result<void> SkinPalette::Set(const std::span<const math::mat4f> matrices, const u64 source_generation) {
    if (matrices.size() != matrices_.size())
        return Err(ErrorCode::ValidationInvalidState, "skin palette matrix count changed");
    if (source_generation < source_generation_)
        return Err(ErrorCode::InvalidState, "stale animation generation cannot update skin palette");
    source_generation_ = source_generation;
    previous_matrices_ = matrices_;
    previous_dirty_ = true;
    for (u32 index = 0; index < matrices.size(); ++index)
        if (matrices_[index] != matrices[index]) {
            matrices_[index] = matrices[index];
            dirty_.Mark(static_cast<u64>(index) * sizeof(math::mat4f), sizeof(math::mat4f));
        }
    if (!dirty_.Empty())
        ++content_version_;
    return Ok();
}

Result<void> SkinPalette::Upload(BufferPool& pool, UploadScheduler& uploads) {
    if (slice_.allocation.IsValid() == false)
        TRY_ASSIGN(slice_, pool.Allocate(static_cast<u64>(matrices_.size()) * sizeof(math::mat4f), 256));
    if (previous_slice_.allocation.IsValid() == false)
        TRY_ASSIGN(
            previous_slice_,
            pool.Allocate(static_cast<u64>(previous_matrices_.size()) * sizeof(math::mat4f), 256)
        );
    if (publication_) {
        const auto residency = publication_->Snapshot().residency;
        if (residency == ResidencyState::UploadPending)
            return Ok();
        if (residency == ResidencyState::Resident)
            resident_ = true;
    }
    if (dirty_.Empty() && !previous_dirty_)
        return Ok();
    ResidencyRecord residency;
    residency.resource = ResourceState::Ready;
    residency.residency = ResidencyState::UploadPending;
    residency.content_version = woki::gfx::ContentVersion(content_version_);
    residency.residency_version = ResidencyVersion(content_version_);
    std::vector<BufferUploadRequest> requests;
    publication_ = UploadPublicationToken::Create(
        residency,
        static_cast<u32>(dirty_.Ranges().size()) + (previous_dirty_ ? 1U : 0U)
    );
    for (const auto range : dirty_.Ranges()) {
        const auto size = static_cast<size_t>(range.size);
        std::vector<std::byte> bytes(size);
        std::memcpy(
            bytes.data(),
            reinterpret_cast<const std::byte*>(matrices_.data()) + static_cast<size_t>(range.offset),
            size
        );
        requests.push_back({pool.SharedBuffer(), slice_.offset + range.offset, std::move(bytes), publication_});
    }
    if (previous_dirty_) {
        std::vector<std::byte> bytes(previous_matrices_.size() * sizeof(math::mat4f));
        std::memcpy(bytes.data(), previous_matrices_.data(), bytes.size());
        requests.push_back({pool.SharedBuffer(), previous_slice_.offset, std::move(bytes), publication_});
        previous_dirty_ = false;
    }
    if (!requests.empty())
        if (auto queued = uploads.EnqueueBatch(std::move(requests)); !queued) {
            publication_->Fail(std::string(queued.error().Message()));
            return Err(std::move(queued).error());
        }
    dirty_.Clear();
    return Ok();
}

SkinPaletteRegistry::SkinPaletteRegistry(BufferPool& pool, UploadScheduler& uploads)
    : pool_(&pool),
      uploads_(&uploads) {}

SkinPaletteRegistry::~SkinPaletteRegistry() {
    for (u32 index = 0; index < slots_.size(); ++index)
        if (slots_[index].value)
            static_cast<void>(Destroy(SkinPaletteHandle::Create(index, slots_[index].generation)));
}

SkinPaletteHandle SkinPaletteRegistry::Create(const u32 joint_count) {
    const u32 index = static_cast<u32>(slots_.size());
    slots_.push_back({1, createScope<SkinPalette>(joint_count)});
    return SkinPaletteHandle::Create(index, 1);
}

Result<void> SkinPaletteRegistry::Set(
    const SkinPaletteHandle handle,
    const std::span<const math::mat4f> matrices,
    const u64 source_generation
) {
    if (Get(handle) == nullptr)
        return Err(ErrorCode::InvalidState, "skin palette handle is invalid");
    return slots_[handle.Index()].value->Set(matrices, source_generation);
}

Result<void> SkinPaletteRegistry::Upload(const SkinPaletteHandle handle) {
    if (Get(handle) == nullptr)
        return Err(ErrorCode::InvalidState, "skin palette handle is invalid");
    return slots_[handle.Index()].value->Upload(*pool_, *uploads_);
}

const SkinPalette* SkinPaletteRegistry::Get(const SkinPaletteHandle handle) const noexcept {
    return handle.IsValid() && handle.Index() < slots_.size()
                   && slots_[handle.Index()].generation == handle.Generation()
               ? slots_[handle.Index()].value.get()
               : nullptr;
}

void SkinPaletteRegistry::MarkUsed(const SkinPaletteHandle handle, const rhi::SubmissionTicket submission) noexcept {
    if (const auto* palette = Get(handle); palette != nullptr && palette->Slice().allocation.IsValid())
        pool_->MarkUsed(submission);
}

Result<void> SkinPaletteRegistry::Destroy(const SkinPaletteHandle handle, const rhi::SubmissionTicket safe_after) {
    if (Get(handle) == nullptr)
        return Err(ErrorCode::InvalidState, "skin palette handle is invalid");
    auto& slot = slots_[handle.Index()];
    if (slot.value->Slice().allocation.IsValid())
        TRY_VOID(pool_->Free(slot.value->Slice().allocation, safe_after));
    if (slot.value->PreviousSlice().allocation.IsValid())
        TRY_VOID(pool_->Free(slot.value->PreviousSlice().allocation, safe_after));
    slot.value.reset();
    ++slot.generation;
    return Ok();
}

Result<scope<SkinPaletteRegistry>> SkinPaletteRegistry::PrepareReplacement(
    BufferPool& pool,
    UploadScheduler& uploads
) const {
    auto replacement = createScope<SkinPaletteRegistry>(pool, uploads);
    replacement->slots_.clear();
    replacement->slots_.resize(slots_.size());
    for (size_t index = 0; index < slots_.size(); ++index) {
        replacement->slots_[index].generation = slots_[index].generation;
        if (slots_[index].value) {
            const auto matrices = slots_[index].value->Matrices();
            replacement->slots_[index].value = createScope<SkinPalette>(static_cast<u32>(matrices.size()));
            TRY_VOID(replacement->slots_[index].value->Set(matrices, slots_[index].value->ContentVersion()));
            TRY_VOID(replacement->slots_[index].value->Upload(pool, uploads));
        }
    }
    return Ok(std::move(replacement));
}
} // namespace woki::gfx
