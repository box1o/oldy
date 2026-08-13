#pragma once

#include <map>
#include <string>
#include <vector>

#include "descriptor.hpp"

namespace woki::gfx {

struct VariantKey {
    std::vector<std::pair<std::string, PermutationValue>> values;
    ContentHash hash;
};

struct VariantPlan {
    std::vector<VariantKey> variants;
    std::vector<ShaderDiagnostic> diagnostics;
};

#ifndef __EMSCRIPTEN__
[[nodiscard]] VariantPlan PlanVariants(const ShaderDescriptor& descriptor, u64 budget = 256);
[[nodiscard]] Result<VariantKey> MakeVariantKey(
    const ShaderDescriptor& descriptor,
    const std::map<std::string, PermutationValue, std::less<>>& values
);
#endif

} // namespace woki::gfx
