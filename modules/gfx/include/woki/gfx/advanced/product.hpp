#pragma once

#include "source.hpp"
#include "reflection.hpp"

namespace woki::gfx {

inline constexpr u32 kShaderProductType = 0x52444853U; // SHDR in little-endian bytes.
inline constexpr u32 kShaderPayloadVersion = 4;

struct ShaderPayload {
    std::string code;
    ShaderInterface interface;
    std::vector<asset::AssetPath> dependencies;
    std::vector<SourceMapEntry> source_map;
    ContentHash module_hash;
    ContentHash interface_hash;
    ContentHash variant_hash;
};

struct ShaderPayloadLimits {
    u64 max_code_bytes{64U * 1024U * 1024U};
    u32 max_records{65'536};
    u32 max_string_bytes{1024U * 1024U};
};

[[nodiscard]] Result<std::vector<std::byte>> SerializeShaderPayload(
    const ShaderPayload& payload,
    ShaderPayloadLimits limits = {}
);
[[nodiscard]] Result<ShaderPayload> ParseShaderPayload(
    std::span<const std::byte> bytes,
    ShaderPayloadLimits limits = {}
);
[[nodiscard]] Result<asset::Product> MakeShaderProduct(
    asset::AssetId asset_id,
    const ShaderPayload& payload,
    ContentHash source_hash,
    std::vector<asset::ProductDependency> dependencies
);

} // namespace woki::gfx
