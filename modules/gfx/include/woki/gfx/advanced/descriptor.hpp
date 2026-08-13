#pragma once

#include <map>
#include <string>
#include <vector>
#include <variant>

#include "types.hpp"

namespace woki::gfx {

inline constexpr u32 kShaderDescriptorSchema = 2;
using PermutationValue = std::variant<bool, i64, f64, std::string>;

struct EntryPointDesc {
    ShaderStage stage{ShaderStage::Vertex};
    std::string name;
    std::string semantic;
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
    std::map<std::pair<u32, u32>, std::string> binding_semantics;
    std::map<u32, std::string> group_semantics;
    CompileOptions compile_options;
};

struct DescriptorResult {
    ShaderDescriptor descriptor;
    std::vector<ShaderDiagnostic> diagnostics;
};

#ifndef __EMSCRIPTEN__
[[nodiscard]] DescriptorResult ParseShaderDescriptor(const asset::AssetPath& path, std::string_view jsonc);
#endif

} // namespace woki::gfx
