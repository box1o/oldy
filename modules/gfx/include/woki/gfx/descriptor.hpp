#pragma once

#include <map>
#include <string>
#include <variant>
#include <vector>

#include "types.hpp"

namespace woki::gfx {

inline constexpr u32 kShaderDescriptorSchema = 1;
using PermutationValue = std::variant<bool, i64, f64, std::string>;

struct EntryPointDesc {
    ShaderStage stage{ShaderStage::Vertex};
    std::string name;
};

struct PermutationDesc {
    std::string name;
    std::vector<PermutationValue> values;
};

struct CompileOptions {
    bool warnings_as_errors{false};
    bool emit_source_map{true};
};

struct ShaderDescriptor {
    u32 schema{kShaderDescriptorSchema};
    std::string name;
    ShaderLanguage language{ShaderLanguage::Wgsl};
    std::vector<asset::AssetPath> sources;
    std::vector<EntryPointDesc> entry_points;
    std::vector<PermutationDesc> permutations;
    std::vector<std::string> capabilities;
    CompileOptions compile_options;
};

struct DescriptorResult {
    ShaderDescriptor descriptor;
    std::vector<ShaderDiagnostic> diagnostics;
};

[[nodiscard]] DescriptorResult ParseShaderDescriptor(const asset::AssetPath& path, std::string_view jsonc);

} // namespace woki::gfx
