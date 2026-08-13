#pragma once

#include <functional>
#include <map>

#include <woki/asset.hpp>
#include <woki/math.hpp>

#include "types.hpp"
#include "material.hpp"

namespace woki::gfx {

enum class ImportedTopology : u8 { Points, Lines, Triangles, TriangleStrip, TriangleFan, Unsupported };
enum class AnimationPath : u8 { Translation, Rotation, Scale };
enum class AnimationInterpolation : u8 { Linear, Step, CubicSpline };
enum class ImportedAlphaMode : u8 { Opaque, Mask, Blend };

struct ImportedDiagnostic final {
    DiagnosticSeverity severity{DiagnosticSeverity::Error};
    std::string code;
    std::string message;
};

struct SourceConversion final {
    std::string source_coordinate_system;
    std::string canonical_coordinate_system{"right-handed,+Y-up,-Z-forward"};
    f32 source_units_per_meter{1.0F};
    f32 applied_scale{1.0F};
    bool reflected{};
};

struct ImportedTextureRef final {
    std::string semantic;
    std::string uri;
    u32 image{};
};

struct ImportedImage final {
    std::string uri; // Original authored URI, including data URIs.
    std::string mime_type;
    std::vector<std::byte> payload;
    bool embedded{};
};

struct ImportedMaterial final {
    std::string name;
    math::vec4f base_color{1.0F};
    math::vec3f emissive{};
    f32 metallic{};
    f32 roughness{1.0F};
    f32 opacity{1.0F};
    f32 alpha_cutoff{0.5F};
    f32 normal_scale{1.0F};
    f32 occlusion_strength{1.0F};
    ImportedAlphaMode alpha_mode{ImportedAlphaMode::Opaque};
    bool double_sided{};
    std::vector<ImportedTextureRef> textures;
};

struct ImportedInfluence final {
    u32 joint{};
    f32 weight{};
};

struct ImportedPrimitive final {
    std::string name;
    ImportedTopology topology{ImportedTopology::Triangles};
    u32 material_slot{};
    std::vector<math::vec3f> positions;
    std::vector<math::vec3f> normals;
    std::vector<math::vec4f> tangents;
    std::vector<math::vec2f> uv0;
    std::vector<math::vec2f> uv1;
    std::vector<math::vec4f> colors;
    std::vector<std::array<ImportedInfluence, 4>> influences;
    std::vector<u32> indices;
    math::vec3f bounds_min{};
    math::vec3f bounds_max{};
};

struct ImportedMesh final {
    std::string name;
    std::optional<u32> skin;
    std::vector<ImportedPrimitive> primitives;
};

struct ImportedNode final {
    std::string name;
    i32 parent{-1};
    std::vector<u32> children;
    std::vector<u32> meshes;
    std::optional<u32> skin;
    math::mat4f local{math::mat4f::identity()};
    math::mat4f world{math::mat4f::identity()};
};

struct ImportedJoint final {
    std::string name;
    u32 node{};
    i32 parent{-1};
    math::mat4f inverse_bind{math::mat4f::identity()};
};

struct ImportedSkeleton final {
    std::string name;
    std::vector<ImportedJoint> joints;
};

struct ImportedSkin final {
    std::string name;
    u32 skeleton{};
    std::vector<u32> joints;
};

struct ImportedAnimationChannel final {
    u32 node{};
    AnimationPath path{AnimationPath::Translation};
    AnimationInterpolation interpolation{AnimationInterpolation::Linear};
    std::vector<f32> times;
    std::vector<math::vec4f> values;
};

struct ImportedAnimationClip final {
    std::string name;
    f32 duration{};
    f32 source_ticks_per_second{};
    std::vector<ImportedAnimationChannel> channels;
};

struct ImportedScene final {
    std::string name;
    SourceConversion conversion;
    std::vector<ImportedMesh> meshes;
    std::vector<ImportedMaterial> materials;
    std::vector<ImportedImage> images;
    std::vector<ImportedNode> nodes;
    std::vector<ImportedSkeleton> skeletons;
    std::vector<ImportedSkin> skins;
    std::vector<ImportedAnimationClip> animations;
    std::vector<asset::AssetUri> dependencies;
    std::vector<ImportedDiagnostic> diagnostics;
};

#ifndef __EMSCRIPTEN__
using ModelDependencyResolver = std::function<Result<std::vector<std::byte>>(const asset::AssetUri&)>;

struct ModelImportRequest final {
    asset::AssetUri source_uri;
    std::string mime_type;
    std::span<const std::byte> bytes;
    ModelDependencyResolver resolve;
    f32 unit_scale{1.0F};
};

class ModelImporter {
public:
    virtual ~ModelImporter() = default;
    [[nodiscard]] virtual std::string_view Name() const noexcept = 0;
    [[nodiscard]] virtual bool Supports(std::string_view extension, std::string_view mime_type) const noexcept = 0;
    [[nodiscard]] virtual Result<ImportedScene> Import(const ModelImportRequest& request) const = 0;
};

class ModelImporterRegistry final {
public:
    [[nodiscard]] Result<void> Register(ref<const ModelImporter> importer);
    [[nodiscard]] const ModelImporter* Select(
        std::string_view extension,
        std::string_view mime_type,
        std::string_view preferred = "auto"
    ) const noexcept;

private:
    std::vector<ref<const ModelImporter>> importers_;
};

struct GeneratedImportedTexture final {
    asset::AssetId id;
    TextureSemantic semantic{TextureSemantic::Color};
    TextureColorSpace color_space{TextureColorSpace::Srgb};
    std::optional<asset::AssetUri> source;
    std::string mime_type;
    std::vector<std::byte> embedded_bytes;
};

struct GeneratedImportedMaterial final {
    u32 slot{};
    MaterialTypeSource definition;
    MaterialInstanceSource instance;
    std::vector<GeneratedImportedTexture> textures;
    std::vector<asset::AssetUri> dependencies;
};

[[nodiscard]] Result<std::vector<GeneratedImportedMaterial>> BuildImportedMaterials(
    const ImportedScene& scene,
    asset::AssetId model_id
);

[[nodiscard]] ref<const ModelImporter> CreateFastGltfModelImporter();
[[nodiscard]] ref<const ModelImporter> CreateAssimpModelImporter();
[[nodiscard]] Result<ModelImporterRegistry> CreateDefaultModelImporterRegistry();

[[nodiscard]] Result<void> NormalizeInfluences(ImportedScene& scene);
#endif

} // namespace woki::gfx
