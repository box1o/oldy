#pragma once

#include <array>
#include <map>
#include <variant>

#include "pipeline.hpp"
#include "texture.hpp"
#include "../handles.hpp"

namespace woki::gfx {

inline constexpr u32 kMaterialTypeSchema = 1;
inline constexpr u32 kMaterialInstanceSchema = 1;
inline constexpr u32 kMaterialGroup = 2;
inline constexpr u32 kMaxMaterialProperties = 256;
inline constexpr u32 kMaxMaterialOverrides = 16;
inline constexpr u32 kMaxMaterialPermutations = 256;

class MaterialPropertyId final {
public:
    constexpr MaterialPropertyId() noexcept = default;

    explicit constexpr MaterialPropertyId(u32 value) noexcept
        : value_(value) {}

    [[nodiscard]] static MaterialPropertyId FromName(std::string_view name) noexcept;

    [[nodiscard]] constexpr u32 Value() const noexcept {
        return value_;
    }

    [[nodiscard]] constexpr explicit operator bool() const noexcept {
        return value_ != 0;
    }

    [[nodiscard]] friend constexpr auto operator<=>(const MaterialPropertyId&, const MaterialPropertyId&) noexcept = default;

private:
    u32 value_{};
};

enum class MaterialValueType : u8 { Bool, I32, U32, F32, Vec2, Vec3, Vec4, Mat4, Texture2D, TextureCube, Sampler };
using MaterialValue = std::variant<bool, i32, u32, f32, std::array<f32, 2>, std::array<f32, 3>, std::array<f32, 4>, std::array<f32, 16>, asset::AssetId>;
enum class MaterialPass : u8 { Forward, Depth, Shadow, Velocity };
enum class MaterialBlendMode : u8 { Opaque, Alpha, Additive };

struct MaterialRenderState final {
    MaterialBlendMode blend{MaterialBlendMode::Opaque};
    rhi::CullMode cull{rhi::CullMode::Back};
    rhi::CompareFunction depth_compare{rhi::CompareFunction::LessEqual};
    bool depth_write{true};
    bool alpha_test{};
    [[nodiscard]] friend auto operator<=>(const MaterialRenderState&, const MaterialRenderState&) noexcept = default;
};

struct MaterialPropertySource final {
    MaterialPropertyId id;
    std::string name;
    MaterialValueType type{MaterialValueType::F32};
    MaterialValue default_value{0.0F};
    std::optional<u32> offset;
};

struct MaterialTextureSource final {
    MaterialPropertyId id;
    std::string name;
    MaterialValueType type{MaterialValueType::Texture2D};
    u32 binding{};
    TextureSemantic semantic{TextureSemantic::Color};
    asset::AssetId default_asset;
};

struct MaterialSamplerSource final {
    MaterialPropertyId id;
    std::string name;
    u32 binding{};
    TextureSamplerDefaults defaults;
};

struct MaterialOverrideSource final {
    MaterialPropertyId property;
    std::string name;
    u32 override_id{};
    std::vector<MaterialValue> values;
};

struct MaterialTypeSource final {
    u32 schema{kMaterialTypeSchema};
    asset::AssetId id;
    std::string name;
    asset::AssetId shader;
    std::string product_family;
    std::vector<MaterialPass> passes;
    std::map<MaterialPass, std::pair<std::string, std::string>> entry_points;
    MaterialRenderState render_state;
    std::vector<MaterialPropertySource> properties;
    std::vector<MaterialTextureSource> textures;
    std::vector<MaterialSamplerSource> samplers;
    std::vector<MaterialOverrideSource> overrides;
};

struct MaterialInstanceSource final {
    u32 schema{kMaterialInstanceSchema};
    asset::AssetId id;
    asset::AssetId type;
    std::map<MaterialPropertyId, MaterialValue> overrides;
};

[[nodiscard]] Result<MaterialTypeSource> ParseMaterialType(std::string_view jsonc);
[[nodiscard]] Result<MaterialInstanceSource> ParseMaterialInstance(std::string_view jsonc, const MaterialTypeSource& type);
[[nodiscard]] bool MaterialValueMatches(MaterialValueType type, const MaterialValue& value) noexcept;

} // namespace woki::gfx

template <>
struct std::hash<woki::gfx::MaterialPropertyId> {
    size_t operator()(woki::gfx::MaterialPropertyId id) const noexcept {
        return std::hash<woki::u32>{}(id.Value());
    }
};
