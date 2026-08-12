#include <woki/gfx/compiler.hpp>

namespace woki::gfx {

#ifndef WOKI_GFX_HAS_TINT
namespace {
class UnavailableCompiler final : public ShaderCompiler {
public:
    CompileOutput Compile(const CompileRequest& request) const override {
        CompileOutput output;
        output.code = request.source.code;
        output.diagnostics.push_back({"SHD3001", DiagnosticSeverity::Error, "Tint compiler support is not available in this host build", {}, {}});
        return output;
    }
};
} // namespace

scope<ShaderCompiler> CreateTintShaderCompiler() {
    return createScope<UnavailableCompiler>();
}

bool IsTintShaderCompilerAvailable() noexcept {
    return false;
}
#endif

} // namespace woki::gfx
