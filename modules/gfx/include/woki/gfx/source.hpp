#pragma once

#include <string>
#include <vector>

#include "descriptor.hpp"

namespace woki::rhi {
class Device;
class ShaderModule;
} // namespace woki::rhi

namespace woki::gfx {

struct SourceMapEntry {
    u64 generated_begin{0};
    u64 generated_end{0};
    asset::AssetPath source;
    u64 source_begin{0};
    u64 source_end{0};
};

struct ComposedSource {
    std::string code;
    std::vector<asset::AssetPath> dependencies;
    std::vector<SourceMapEntry> source_map;
    std::vector<ShaderDiagnostic> diagnostics;
};

class ShaderSourceResolver {
public:
    explicit ShaderSourceResolver(const asset::Vfs& vfs)
        : vfs_(vfs) {}

    [[nodiscard]] ComposedSource Compose(const ShaderDescriptor& descriptor, std::optional<asset::AssetPath> descriptor_path = std::nullopt) const;

private:
    const asset::Vfs& vfs_;
};

[[nodiscard]] SourceRange MapGeneratedRange(const ComposedSource& source, u64 offset, u64 length);
[[nodiscard]] Result<ref<rhi::ShaderModule>> CreateShaderModuleFromAsset(rhi::Device& device, const asset::Vfs& vfs, const asset::AssetPath& descriptor_path);

} // namespace woki::gfx
