#pragma once

#include <compare>

#include <woki/core.hpp>

namespace woki::gfx {

struct MeshTag;
struct MaterialInstanceTag;
struct SkinPaletteTag;
struct PipelineAssetTag;
struct TextureTag;
struct SceneTag;
struct SkeletonTag;
struct AnimationClipTag;
struct AnimationPlaybackTag;

using MeshHandle = Handle<MeshTag>;
using MaterialInstanceHandle = Handle<MaterialInstanceTag>;
using SkinPaletteHandle = Handle<SkinPaletteTag>;
using PipelineHandle = Handle<PipelineAssetTag>;
using TextureHandle = Handle<TextureTag>;
using SceneHandle = Handle<SceneTag>;
using SkeletonHandle = Handle<SkeletonTag>;
using AnimationClipHandle = Handle<AnimationClipTag>;
using AnimationPlaybackHandle = Handle<AnimationPlaybackTag>;

class GpuSubmissionId final {
public:
    constexpr GpuSubmissionId() noexcept = default;

    explicit constexpr GpuSubmissionId(u64 value) noexcept
        : value_(value) {}

    [[nodiscard]] constexpr bool IsValid() const noexcept {
        return value_ != 0;
    }

    [[nodiscard]] constexpr u64 Value() const noexcept {
        return value_;
    }

    [[nodiscard]] friend constexpr auto operator<=>(const GpuSubmissionId&, const GpuSubmissionId&) = default;

private:
    u64 value_{};
};

enum class MeshState : u8 { Unloaded, Loading, CpuReady, Uploading, Resident, Failed, Evicted, Lost };
enum class MaterialPhase : u8 { Opaque, AlphaTest, Transparent };

} // namespace woki::gfx
