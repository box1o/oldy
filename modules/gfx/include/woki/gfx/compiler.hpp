#pragma once

#include "reflection.hpp"
#include "source.hpp"

namespace woki::gfx {

struct CompileRequest {
    const ShaderDescriptor& descriptor;
    const ComposedSource& source;
};

struct CompileOutput {
    std::string code;
    ShaderInterface interface;
    std::vector<ShaderDiagnostic> diagnostics;
    bool validated_with_tint{false};
};

class ShaderCompiler {
public:
    virtual ~ShaderCompiler() = default;
    [[nodiscard]] virtual CompileOutput Compile(const CompileRequest& request) const = 0;
};

[[nodiscard]] scope<ShaderCompiler> CreateTintShaderCompiler();
[[nodiscard]] bool IsTintShaderCompilerAvailable() noexcept;

} // namespace woki::gfx
