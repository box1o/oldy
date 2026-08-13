#pragma once

#include <woki/math.hpp>

#include "buffer_pool.hpp"
#include "mesh_product.hpp"
#include "upload.hpp"
#include "../handles.hpp"

namespace woki::gfx {

struct SkeletonTag;
struct AnimationClipTag;
using SkeletonHandle = Handle<SkeletonTag>;
using AnimationClipHandle = Handle<AnimationClipTag>;

struct SkeletonAsset final {
    u64 generation{};
    std::vector<MeshJoint> joints;
    std::vector<math::mat4f> animation_prefixes;
    std::vector<u32> joint_nodes;
    std::vector<i32> node_parents;
    std::vector<math::mat4f> node_locals;
    std::vector<math::mat4f> node_prefixes;
};

struct AnimationClip final {
    u64 generation{};
    std::string name;
    u32 skeleton{};
    f32 duration{};
    std::vector<MeshAnimationChannel> channels;
};

struct AnimationState final {
    u32 clip{};
    f32 time{};
    f32 speed{1.0F};
    bool loop{true};
    bool paused{};
};

class SkeletonRegistry final {
public:
    [[nodiscard]] SkeletonHandle Register(const MeshProduct& product, u64 generation);
    [[nodiscard]] std::vector<SkeletonHandle> RegisterAll(const MeshProduct& product, u64 generation);
    [[nodiscard]] ref<const SkeletonAsset> Get(SkeletonHandle handle) const noexcept;

private:
    struct Slot {
        u32 generation{1};
        ref<const SkeletonAsset> value;
    };

    std::vector<Slot> slots_;
};

class AnimationClipRegistry final {
public:
    [[nodiscard]] std::vector<AnimationClipHandle> Register(const MeshProduct& product, u64 generation);
    [[nodiscard]] ref<const AnimationClip> Get(AnimationClipHandle handle) const noexcept;

private:
    struct Slot {
        u32 generation{1};
        ref<const AnimationClip> value;
    };

    std::vector<Slot> slots_;
};

class Animator final {
public:
    explicit Animator(ref<const SkeletonAsset> skeleton, math::mat4f mesh_transform = math::mat4f::identity());
    void SetClips(std::vector<ref<const AnimationClip>> clips);
    [[nodiscard]] Result<void> Select(u32 clip, bool loop = true);
    void Update(f32 delta_seconds);
    [[nodiscard]] Result<std::vector<math::mat4f>> Evaluate() const;

    [[nodiscard]] AnimationState& State() noexcept {
        return state_;
    }

    [[nodiscard]] const AnimationState& State() const noexcept {
        return state_;
    }

private:
    ref<const SkeletonAsset> skeleton_;
    math::mat4f inverse_mesh_transform_{math::mat4f::identity()};
    std::vector<ref<const AnimationClip>> clips_;
    AnimationState state_;
};

class SkinPalette final {
public:
    explicit SkinPalette(u32 joint_count);
    [[nodiscard]] Result<void> Set(std::span<const math::mat4f> matrices, u64 source_generation);
    [[nodiscard]] Result<void> Upload(BufferPool& pool, UploadScheduler& uploads);

    void MarkDeviceLost() noexcept {
        slice_ = {};
        previous_slice_ = {};
        if (publication_)
            publication_->MarkDeviceLost();
        publication_.reset();
        dirty_.Mark(0, static_cast<u64>(matrices_.size()) * sizeof(math::mat4f));
        previous_dirty_ = true;
        resident_ = false;
    }

    [[nodiscard]] u64 ContentVersion() const noexcept {
        return content_version_;
    }

    [[nodiscard]] const BufferSlice& Slice() const noexcept {
        return slice_;
    }

    [[nodiscard]] const BufferSlice* DrawableSlice() const noexcept {
        return resident_ || (publication_ && publication_->Snapshot().residency == ResidencyState::Resident) ? &slice_
                                                                                                             : nullptr;
    }

    [[nodiscard]] const BufferSlice* PreviousDrawableSlice() const noexcept {
        return resident_ || (publication_ && publication_->Snapshot().residency == ResidencyState::Resident)
                   ? &previous_slice_
                   : nullptr;
    }

    [[nodiscard]] const BufferSlice& PreviousSlice() const noexcept {
        return previous_slice_;
    }

    [[nodiscard]] std::span<const math::mat4f> Matrices() const noexcept {
        return matrices_;
    }

private:
    std::vector<math::mat4f> matrices_;
    std::vector<math::mat4f> previous_matrices_;
    DirtyRangeSet dirty_;
    BufferSlice slice_;
    BufferSlice previous_slice_;
    bool previous_dirty_{true};
    bool resident_{};
    u64 content_version_{1};
    u64 source_generation_{};
    std::shared_ptr<UploadPublicationToken> publication_;
};

class SkinPaletteRegistry final {
public:
    SkinPaletteRegistry(BufferPool& pool, UploadScheduler& uploads);
    ~SkinPaletteRegistry();
    [[nodiscard]] SkinPaletteHandle Create(u32 joint_count);
    [[nodiscard]] Result<void> Set(
        SkinPaletteHandle handle,
        std::span<const math::mat4f> matrices,
        u64 source_generation
    );
    [[nodiscard]] Result<void> Upload(SkinPaletteHandle handle);
    [[nodiscard]] const SkinPalette* Get(SkinPaletteHandle handle) const noexcept;
    void MarkUsed(SkinPaletteHandle handle, rhi::SubmissionTicket submission) noexcept;
    [[nodiscard]] Result<void> Destroy(SkinPaletteHandle handle, rhi::SubmissionTicket safe_after = {});
    [[nodiscard]] Result<scope<SkinPaletteRegistry>> PrepareReplacement(
        BufferPool& pool,
        UploadScheduler& uploads
    ) const;

private:
    struct Slot {
        u32 generation{1};
        scope<SkinPalette> value;
    };

    BufferPool* pool_;
    UploadScheduler* uploads_;
    std::vector<Slot> slots_;
};

} // namespace woki::gfx
