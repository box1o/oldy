#pragma once

#include "pipeline.hpp"

namespace woki::gfx {

inline constexpr u32 kPipelineProductType = 0x45504950U; // PIPE in little-endian bytes.
inline constexpr u32 kPipelineProductVersion = 4;

struct PipelineProductLimits {
    u32 max_records{65'536};
    u32 max_string_bytes{1024U * 1024U};
    u32 max_total_string_bytes{16U * 1024U * 1024U};
    u32 max_payload_bytes{64U * 1024U * 1024U};
};

[[nodiscard]] Result<std::vector<std::byte>> SerializePipeline(const RenderPipelineIR& pipeline, PipelineProductLimits limits = {});
[[nodiscard]] Result<RenderPipelineIR> ParsePipelineProduct(std::span<const std::byte> bytes, PipelineProductLimits limits = {});
[[nodiscard]] Result<asset::Product> MakePipelineProduct(const RenderPipelineIR& pipeline);
[[nodiscard]] Result<asset::Product> CookPipeline(const asset::Vfs& vfs, const FeatureRegistry& registry, const asset::AssetPath& path, std::vector<PipelineDiagnostic>* diagnostics = nullptr);

} // namespace woki::gfx
