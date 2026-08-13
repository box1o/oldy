#pragma once

#include "model_import.hpp"
#include "reflection.hpp"

namespace woki::gfx {

inline constexpr u32 kMeshSourceType = 0x4352534dU;  // MSRC
inline constexpr u32 kMeshProductType = 0x5250534dU; // MSPR

enum class MeshImporterKind : u8 { Auto, FastGltf, Assimp };
enum class TangentPolicy : u8 { Preserve, GenerateIfMissing, Generate, None };
enum class VertexSemantic : u8 { Position, Normal, Tangent, TexCoord0, TexCoord1, Color0, Joints0, Weights0 };
enum class VertexFormat : u8 { Float32x2, Float32x3, Float32x4, Uint16x4, Unorm16x4 };
enum class VertexStepMode : u8 { Vertex, Instance };

struct VertexAttributeDesc final {
    VertexSemantic semantic{VertexSemantic::Position};
    u32 location{};
    VertexFormat format{VertexFormat::Float32x3};
    u32 offset{};
    [[nodiscard]] friend auto operator<=>(const VertexAttributeDesc&, const VertexAttributeDesc&) noexcept = default;
};

struct VertexStreamDesc final {
    u32 stride{};
    VertexStepMode step_mode{VertexStepMode::Vertex};
    std::vector<VertexAttributeDesc> attributes;
    [[nodiscard]] friend auto operator<=>(const VertexStreamDesc&, const VertexStreamDesc&) noexcept = default;
};

struct VertexSchema final {
    u64 id{};
    std::vector<VertexStreamDesc> streams;
};

struct MeshletOptions final {
    bool enabled{true};
    u32 max_vertices{64};
    u32 max_triangles{124};
};

struct MeshSource final {
    MeshSource(asset::AssetId id, asset::AssetUri uri)
        : asset_id(std::move(id)),
          source_uri(std::move(uri)) {}

    u32 schema{1};
    asset::AssetId asset_id;
    asset::AssetUri source_uri;
    MeshImporterKind importer{MeshImporterKind::Auto};
    f32 unit_scale{1.0F};
    TangentPolicy tangents{TangentPolicy::GenerateIfMissing};
    bool optimize{true};
    std::vector<f32> lod_ratios{1.0F};
    std::vector<f32> lod_errors;
    MeshletOptions meshlets;
    bool quantize{};
    bool compress{};
    bool import_animations{true};
    std::vector<std::string> animations;
};

[[nodiscard]] Result<MeshSource> ParseMeshSource(std::string_view jsonc);
[[nodiscard]] u64 HashVertexSchema(const VertexSchema& schema) noexcept;
[[nodiscard]] Result<void> ValidateVertexSchema(const VertexSchema& schema, const ShaderInterface& shader, std::string_view entry_point);

#ifndef __EMSCRIPTEN__
class MeshBuilder final : public asset::AssetBuilder {
public:
    using ResolveProduct = std::function<Result<asset::Product>(asset::AssetId)>;
    MeshBuilder(ref<const asset::Vfs> vfs, ref<const ModelImporterRegistry> importers, ResolveProduct resolve = {});
    [[nodiscard]] const asset::BuilderDescriptor& Descriptor() const noexcept override;
    [[nodiscard]] Result<asset::Product> Build(const asset::BuildRequest& request, asset::BuildContext& context, std::span<const std::byte> source) const override;

private:
    ref<const asset::Vfs> vfs_;
    ref<const ModelImporterRegistry> importers_;
    ResolveProduct resolve_;
    asset::BuilderDescriptor descriptor_;
};

[[nodiscard]] Result<void> RegisterMeshAssetBuilder(asset::AssetServices& services, ref<const asset::Vfs> vfs, ref<const ModelImporterRegistry> importers, MeshBuilder::ResolveProduct resolve = {});
#endif

} // namespace woki::gfx
