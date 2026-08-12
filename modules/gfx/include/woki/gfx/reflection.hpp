#pragma once

#include <array>
#include <string>
#include <vector>

#include "types.hpp"

namespace woki::gfx {

enum class ResourceKind : u8 { UniformBuffer, StorageBuffer, ReadOnlyStorageBuffer, Sampler, ComparisonSampler, SampledTexture, MultisampledTexture, StorageTexture, DepthTexture, ExternalTexture };
enum class TextureDimension : u8 { None, D1, D2, D2Array, D3, Cube, CubeArray };
enum class SampleType : u8 { None, Float, UnfilterableFloat, Sint, Uint, Depth };
enum class StorageAccess : u8 { None, WriteOnly, ReadOnly, ReadWrite };
enum class StorageFormat : u8 {
    None,
    R32Uint,
    R32Sint,
    R32Float,
    RG32Uint,
    RG32Sint,
    RG32Float,
    RGBA8Unorm,
    RGBA8Snorm,
    RGBA8Uint,
    RGBA8Sint,
    BGRA8Unorm,
    RGBA16Uint,
    RGBA16Sint,
    RGBA16Float,
    RGBA32Uint,
    RGBA32Sint,
    RGBA32Float
};
enum class ValueType : u8 { Unknown, Bool, F16, F32, I32, U32, Vec2F, Vec3F, Vec4F, Vec2I, Vec3I, Vec4I, Vec2U, Vec3U, Vec4U };

struct BindingInfo {
    u32 group{0};
    u32 binding{0};
    u8 stages{0};
    ResourceKind kind{ResourceKind::UniformBuffer};
    TextureDimension dimension{TextureDimension::None};
    SampleType sample_type{SampleType::None};
    StorageAccess storage_access{StorageAccess::None};
    StorageFormat storage_format{StorageFormat::None};
    u64 min_binding_size{0};
    u32 array_size{0};
    [[nodiscard]] friend bool operator==(const BindingInfo&, const BindingInfo&) = default;
};

struct StageIo {
    u32 location{0};
    ValueType type{ValueType::Unknown};
};

struct OverrideInfo {
    std::string name;
    u32 id{0};
    ValueType type{ValueType::Unknown};
    bool has_default{false};
};

struct EntryPointInfo {
    std::string name;
    ShaderStage stage{ShaderStage::Vertex};
    std::vector<StageIo> inputs;
    std::vector<StageIo> outputs;
    std::array<u32, 3> workgroup_size{1, 1, 1};
};

struct ShaderInterface {
    std::vector<EntryPointInfo> entry_points;
    std::vector<BindingInfo> bindings;
    std::vector<OverrideInfo> overrides;
    std::vector<std::string> capabilities;
    ContentHash hash;
};

void NormalizeInterface(ShaderInterface& interface);
[[nodiscard]] ContentHash HashInterface(const ShaderInterface& interface);

} // namespace woki::gfx
