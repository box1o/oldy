#pragma once

#include <woki/core.hpp>
#include <woki/gfx/generated/render_abi.hpp>

#include "layout.hpp"

namespace woki::gfx::abi {

inline constexpr u32 kFrameGroup = 0;
inline constexpr u32 kViewGroup = 1;
inline constexpr u32 kMaterialGroup = 2;
inline constexpr u32 kObjectGroup = 3;
inline constexpr u32 kFrameBinding = 0;
inline constexpr u32 kSceneBinding = 1;
inline constexpr u32 kLocalLightsBinding = 2;
inline constexpr u32 kClusterGridBinding = 3;
inline constexpr u32 kClusterIndicesBinding = 4;
inline constexpr u32 kClusterParamsBinding = 5;
inline constexpr u32 kShadowAtlasBinding = 6;
inline constexpr u32 kShadowSamplerBinding = 7;
inline constexpr u32 kShadowDataBinding = 8;
inline constexpr u32 kEnvironmentRadianceBinding = 9;
inline constexpr u32 kEnvironmentIrradianceBinding = 10;
inline constexpr u32 kEnvironmentPrefilterBinding = 11;
inline constexpr u32 kEnvironmentBrdfBinding = 12;
inline constexpr u32 kEnvironmentSamplerBinding = 13;
inline constexpr u32 kEnvironmentDataBinding = 14;
inline constexpr u32 kViewBinding = 0;
inline constexpr u32 kObjectBinding = 0;
inline constexpr u32 kSkinBinding = 1;
inline constexpr u32 kPreviousSkinBinding = 2;
inline constexpr u32 kAbiGroupCount = 4;

// Object records share one aligned upload buffer. Frame and view bindings are
// rewritten once per submitted view, while material and skin resources retain
// stable offsets for their published generation.
inline constexpr std::array<DynamicBufferBindingPolicy, 1> kDynamicBufferPolicy{{{kObjectGroup, kObjectBinding}}};

} // namespace woki::gfx::abi
