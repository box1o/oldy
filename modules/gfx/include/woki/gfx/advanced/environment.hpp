#pragma once

#include <functional>

#include "texture_library.hpp"
#include "../view.hpp"

namespace woki::gfx {

// Cooked environment products are separate so sky visibility never controls IBL.
// Radiance is scene-linear; irradiance is diffuse convolution, prefiltered_specular
// contains the roughness mip chain, and brdf_lut is the split-sum integration LUT.
struct EnvironmentAsset final {
    asset::AssetId asset_id;
    TextureHandle radiance;
    TextureHandle irradiance;
    TextureHandle prefiltered_specular;
    TextureHandle brdf_lut;
    u32 specular_mip_count{1};
};

struct ResolvedEnvironment final {
    ResolvedTexture radiance;
    ResolvedTexture irradiance;
    ResolvedTexture prefiltered_specular;
    ResolvedTexture brdf_lut;
    u32 specular_mip_count{1};
    bool fallback{};
};

// Native importers may generate all four products. Runtime builds consume only
// their cooked texture handles and use library-owned black cube/BRDF fallbacks.
using BuildEnvironmentProducts = std::function<Result<EnvironmentAsset>(asset::AssetId source)>;

} // namespace woki::gfx
