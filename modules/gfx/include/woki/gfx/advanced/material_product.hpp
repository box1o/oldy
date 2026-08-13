#pragma once

#include <functional>

#include "layout.hpp"
#include "material.hpp"
#include "product.hpp"
#include "texture_cache.hpp"
#include "texture_product.hpp"

namespace woki::gfx {

inline constexpr u32 kMaterialTypeSourceType = 0x5454414dU;        // MATT
inline constexpr u32 kMaterialInstanceSourceType = 0x4954414dU;    // MATI
inline constexpr u32 kMaterialDefinitionProductType = 0x444d544dU; // MTMD
inline constexpr u32 kMaterialInstanceProductType = 0x494d544dU;   // MTMI
inline constexpr u32 kMaterialProductVersion = 4;

struct MaterialPassProgram final {
    MaterialPass pass{MaterialPass::Forward};
    std::string vertex_entry;
    std::string fragment_entry;
    [[nodiscard]] friend bool operator==(const MaterialPassProgram&, const MaterialPassProgram&) = default;
};

struct MaterialPropertyPlan final {
    MaterialPropertyId id;
    MaterialValueType type{MaterialValueType::F32};
    u32 offset{};
    u32 size{};
    std::vector<std::byte> default_bytes;
    [[nodiscard]] friend bool operator==(const MaterialPropertyPlan&, const MaterialPropertyPlan&) = default;
};

struct MaterialTextureBinding final {
    MaterialPropertyId id;
    MaterialValueType type{MaterialValueType::Texture2D};
    u32 binding{};
    TextureSemantic semantic{TextureSemantic::Color};
    asset::AssetId default_asset;
};

struct MaterialSamplerBinding final {
    MaterialPropertyId id;
    u32 binding{};
    SamplerKey key;
};

struct MaterialVariantInput final {
    MaterialPropertyId property;
    u32 override_id{};
    MaterialValueType type{MaterialValueType::Bool};
    std::vector<MaterialValue> allowed_values;
};

struct MaterialBindingPlan final {
    ContentHash interface_hash;
    PipelineLayoutKey pipeline_layout;
    u32 parameter_binding{};
    u32 parameter_size{};
    std::vector<MaterialPropertyPlan> properties;
    std::vector<MaterialTextureBinding> textures;
    std::vector<MaterialSamplerBinding> samplers;
    std::vector<MaterialVariantInput> variants;
    MaterialPhase phase{MaterialPhase::Opaque};
    MaterialRenderState render_state;
    ContentHash render_state_identity;
};

struct MaterialDefinitionProduct final {
    asset::AssetId id;
    std::string name;
    asset::AssetId shader;
    ContentHash shader_product_hash;
    ContentHash shader_variant_hash;
    std::string product_family;
    std::vector<MaterialPass> passes;
    std::vector<MaterialPassProgram> programs;
    MaterialBindingPlan binding_plan;
};

struct MaterialInstanceProduct final {
    asset::AssetId id;
    asset::AssetId definition;
    ContentHash definition_product_hash;
    std::map<MaterialPropertyId, MaterialValue> overrides;
    std::map<MaterialPropertyId, MaterialValueType> override_types;
};

struct MaterialCompileResult final {
    std::optional<MaterialDefinitionProduct> definition;
    std::vector<ShaderDiagnostic> diagnostics;

    [[nodiscard]] bool Valid() const noexcept {
        return definition.has_value();
    }
};

[[nodiscard]] MaterialCompileResult CompileMaterialType(const MaterialTypeSource& source, const ShaderPayload& shader, ContentHash shader_product_hash);
[[nodiscard]] Result<MaterialInstanceProduct> CompileMaterialInstance(const MaterialInstanceSource& source, const MaterialDefinitionProduct& definition, ContentHash definition_product_hash);
[[nodiscard]] Result<std::vector<std::byte>> SerializeMaterialDefinition(const MaterialDefinitionProduct& product);
[[nodiscard]] Result<MaterialDefinitionProduct> ParseMaterialDefinition(std::span<const std::byte> bytes);
[[nodiscard]] Result<std::vector<std::byte>> SerializeMaterialInstance(const MaterialInstanceProduct& product);
[[nodiscard]] Result<MaterialInstanceProduct> ParseMaterialInstanceProduct(std::span<const std::byte> bytes);
[[nodiscard]] Result<asset::Product> MakeMaterialDefinitionProduct(const MaterialDefinitionProduct& definition, ContentHash source_hash, std::vector<asset::ProductDependency> dependencies);
[[nodiscard]] Result<asset::Product> MakeMaterialInstanceProduct(const MaterialInstanceProduct& instance, ContentHash source_hash, std::vector<asset::ProductDependency> dependencies);

class MaterialTypeBuilder final : public asset::AssetBuilder {
public:
    using ResolveProduct = std::function<Result<asset::Product>(asset::AssetId)>;
    explicit MaterialTypeBuilder(ResolveProduct resolve);
    [[nodiscard]] const asset::BuilderDescriptor& Descriptor() const noexcept override;
    [[nodiscard]] Result<asset::Product> Build(const asset::BuildRequest&, asset::BuildContext&, std::span<const std::byte>) const override;

private:
    ResolveProduct resolve_;
    asset::BuilderDescriptor descriptor_;
};

class MaterialInstanceBuilder final : public asset::AssetBuilder {
public:
    using ResolveProduct = std::function<Result<asset::Product>(asset::AssetId)>;
    explicit MaterialInstanceBuilder(ResolveProduct resolve);
    [[nodiscard]] const asset::BuilderDescriptor& Descriptor() const noexcept override;
    [[nodiscard]] Result<asset::Product> Build(const asset::BuildRequest&, asset::BuildContext&, std::span<const std::byte>) const override;

private:
    ResolveProduct resolve_;
    asset::BuilderDescriptor descriptor_;
};

[[nodiscard]] Result<void> RegisterMaterialBuilders(asset::BuilderRegistry& registry, MaterialTypeBuilder::ResolveProduct resolve);

// Registers all platform-available gfx builders into the application's explicit asset services.
[[nodiscard]] Result<void> RegisterGraphicsAssetBuilders(asset::AssetServices& services, ref<const asset::Vfs> vfs, MaterialTypeBuilder::ResolveProduct resolve);

} // namespace woki::gfx
