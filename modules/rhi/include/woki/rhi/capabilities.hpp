#pragma once

#include <span>
#include <utility>
#include <vector>

#include "descriptors.hpp"

namespace woki::rhi {

enum class CapabilityFeature : u64 {
    None = 0,
    Compute = 1ULL << 0,
    TimestampQueries = 1ULL << 1,
    IndirectDraw = 1ULL << 2,
    IndirectCount = 1ULL << 3,
    StorageBuffers = 1ULL << 4,
    StorageTextures = 1ULL << 5,
    Readback = 1ULL << 6,
    Presentation = 1ULL << 7,
    HdrPresentation = 1ULL << 8,
    // Compute-written indirect arguments are separated from basic CPU-authored
    // indirect draws because some compatibility backends expose only the latter.
    GpuDrivenIndirect = 1ULL << 9,
};

enum class QueueClass : u8 { Graphics, Compute, Transfer, Present };
enum class TextureCompression : u8 { BC, ETC2, ASTC };

struct TextureFormatCapabilities final {
    TextureFormat format{TextureFormat::Undefined};
    TextureUsage usages{TextureUsage::None};
    std::vector<u32> sample_counts{};
    bool filterable{};
    bool blendable{};
};

class DeviceCapabilities final {
public:
    DeviceCapabilities() = default;

    DeviceCapabilities(CapabilityFeature features, Limits limits, std::vector<TextureFormatCapabilities> formats, std::vector<QueueClass> queues, std::vector<TextureCompression> compression)
        : features_(features),
          limits_(limits),
          formats_(std::move(formats)),
          queues_(std::move(queues)),
          compression_(std::move(compression)) {}

    [[nodiscard]] bool Has(CapabilityFeature feature) const noexcept {
        return (static_cast<u64>(features_) & static_cast<u64>(feature)) != 0;
    }

    [[nodiscard]] const Limits& GetLimits() const noexcept {
        return limits_;
    }

    [[nodiscard]] std::span<const TextureFormatCapabilities> Formats() const noexcept {
        return formats_;
    }

    [[nodiscard]] std::span<const QueueClass> Queues() const noexcept {
        return queues_;
    }

    [[nodiscard]] std::span<const TextureCompression> Compression() const noexcept {
        return compression_;
    }

    [[nodiscard]] const TextureFormatCapabilities* Format(TextureFormat format) const noexcept;
    [[nodiscard]] bool Supports(TextureFormat format, TextureUsage usage, u32 sample_count = 1) const noexcept;

private:
    CapabilityFeature features_{CapabilityFeature::None};
    Limits limits_{};
    std::vector<TextureFormatCapabilities> formats_{};
    std::vector<QueueClass> queues_{};
    std::vector<TextureCompression> compression_{};
};

[[nodiscard]] DeviceCapabilities NormalizeCapabilities(const SupportedFeatures& features,
    const Limits& limits,
    std::span<const TextureFormatCapabilities> formats,
    bool presentation = true,
    bool hdr_presentation = false);

} // namespace woki::rhi
