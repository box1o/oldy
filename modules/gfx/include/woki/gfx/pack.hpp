#pragma once

#include <span>

#include "source.hpp"

namespace woki::gfx {

inline constexpr u32 kShaderPackSchema = 1;
inline constexpr u32 kShaderPackIndexVersion = 1;

struct ShaderPackDependency {
    std::string id;
    std::string version;

    [[nodiscard]] friend auto operator<=>(const ShaderPackDependency&, const ShaderPackDependency&) = default;
};

struct ShaderPackEntry {
    std::string name;
    asset::AssetPath descriptor_path;
    ShaderDescriptor descriptor;
    ComposedSource source;
    ContentHash content_hash;
};

struct ShaderPack {
    std::string id;
    std::string version;
    std::vector<ShaderPackDependency> dependencies;
    std::vector<ShaderPackEntry> entries;
    ContentHash content_hash;
};

struct ShaderPackIndexEntry {
    std::string name;
    asset::AssetPath descriptor_path;
    ContentHash content_hash;
    std::vector<asset::AssetPath> dependencies;
};

struct ShaderPackIndex {
    std::string id;
    std::string version;
    std::vector<ShaderPackDependency> dependencies;
    std::vector<ShaderPackIndexEntry> entries;
    ContentHash content_hash;
};

[[nodiscard]] Result<ShaderPack> LoadShaderPack(const asset::Vfs& vfs, const asset::AssetPath& manifest_path);
[[nodiscard]] ShaderPackIndex MakeShaderPackIndex(const ShaderPack& pack);
[[nodiscard]] Result<std::vector<std::byte>> SerializeShaderPackIndex(const ShaderPackIndex& index);
[[nodiscard]] Result<ShaderPackIndex> ParseShaderPackIndex(std::span<const std::byte> bytes);

} // namespace woki::gfx
