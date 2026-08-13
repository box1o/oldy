#include <algorithm>

#include <woki/rhi/capabilities.hpp>

namespace woki::rhi {
namespace {

constexpr CapabilityFeature operator|(CapabilityFeature lhs, CapabilityFeature rhs) noexcept {
    return static_cast<CapabilityFeature>(static_cast<u64>(lhs) | static_cast<u64>(rhs));
}

} // namespace

const TextureFormatCapabilities* DeviceCapabilities::Format(const TextureFormat format) const noexcept {
    const auto found = std::ranges::find(formats_, format, &TextureFormatCapabilities::format);
    return found == formats_.end() ? nullptr : &*found;
}

bool DeviceCapabilities::Supports(const TextureFormat format, const TextureUsage usage, const u32 sample_count) const noexcept {
    const auto* support = Format(format);
    return support != nullptr && (support->usages & usage) == usage && std::ranges::find(support->sample_counts, sample_count) != support->sample_counts.end();
}

DeviceCapabilities NormalizeCapabilities(const SupportedFeatures& features, const Limits& limits, const std::span<const TextureFormatCapabilities> formats, const bool presentation, const bool hdr_presentation) {
    auto normalized = CapabilityFeature::Compute | CapabilityFeature::IndirectDraw | CapabilityFeature::StorageBuffers | CapabilityFeature::Readback;
    if (limits.max_storage_textures_per_shader_stage != 0 && limits.max_storage_textures_per_shader_stage != kLimitU32Undefined)
        normalized = normalized | CapabilityFeature::StorageTextures;
    if (features.Has(FeatureName::TimestampQuery))
        normalized = normalized | CapabilityFeature::TimestampQueries;
    if (features.Has(FeatureName::MultiDrawIndirect))
        normalized = normalized | CapabilityFeature::IndirectCount | CapabilityFeature::GpuDrivenIndirect;
    if (presentation)
        normalized = normalized | CapabilityFeature::Presentation;
    if (hdr_presentation)
        normalized = normalized | CapabilityFeature::HdrPresentation;

    std::vector<TextureCompression> compression;
    if (features.Has(FeatureName::TextureCompressionBC))
        compression.push_back(TextureCompression::BC);
    if (features.Has(FeatureName::TextureCompressionETC2))
        compression.push_back(TextureCompression::ETC2);
    if (features.Has(FeatureName::TextureCompressionASTC))
        compression.push_back(TextureCompression::ASTC);
    std::vector<TextureFormatCapabilities> copied(formats.begin(), formats.end());
    std::ranges::sort(copied, {}, &TextureFormatCapabilities::format);
    return {normalized, limits, std::move(copied), {QueueClass::Graphics, QueueClass::Compute, QueueClass::Transfer, QueueClass::Present}, std::move(compression)};
}

} // namespace woki::rhi
