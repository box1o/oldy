#include <woki/gfx/advanced/mesh_product.hpp>
#include <woki/gfx/advanced/material_product.hpp>
#include <woki/config.hpp>

namespace woki::gfx {

MeshBuilder::MeshBuilder(ref<const asset::Vfs> vfs, ref<const ModelImporterRegistry> importers, ResolveProduct resolve)
    : vfs_(std::move(vfs)),
      importers_(std::move(importers)),
      resolve_(std::move(resolve)),
      descriptor_{asset::AssetId::FromName("woki.gfx.mesh-builder"), 1, kMeshSourceType, kMeshProductType, "MeshBuilder"} {}

const asset::BuilderDescriptor& MeshBuilder::Descriptor() const noexcept {
    return descriptor_;
}

Result<asset::Product> MeshBuilder::Build(const asset::BuildRequest& request, asset::BuildContext& context, const std::span<const std::byte> descriptor_source) const {
    if (vfs_ == nullptr || importers_ == nullptr)
        return Err(ErrorCode::InvalidState, "mesh builder has no VFS or importer registry");
    auto parsed_source = ParseMeshSource({reinterpret_cast<const char*>(descriptor_source.data()), descriptor_source.size()});
    if (!parsed_source)
        return Err(std::move(parsed_source).error());
    MeshSource source = std::move(*parsed_source);
    if (source.asset_id != request.asset_id)
        return Err(ErrorCode::ValidationInvalidState, "mesh descriptor asset id does not match build request");
    std::vector<std::byte> source_bytes;
    TRY_ASSIGN(source_bytes, vfs_->ReadBinary(source.source_uri, 1024U * 1024U * 1024U));
    const ContentHash source_content_hash = Sha256(source_bytes);
    const asset::AssetId source_id = asset::AssetId::FromName(source.source_uri.String());
    TRY_VOID(context.AddSourceDependency(source_id, source_content_hash));
    std::string extension;
    const auto dot = source.source_uri.Path().String().find_last_of('.');
    if (dot != std::string::npos)
        extension = source.source_uri.Path().String().substr(dot);
    const std::string_view preferred = source.importer == MeshImporterKind::FastGltf ? "fastgltf" : source.importer == MeshImporterKind::Assimp ? "assimp" : "auto";
    const ModelImporter* importer = importers_->Select(extension, {}, preferred);
    if (importer == nullptr)
        return Err(ErrorCode::GraphicsUnsupportedApi, "no registered model importer supports the source");
    ModelImportRequest import_request{source.source_uri, {}, source_bytes, [this](const asset::AssetUri& uri) { return vfs_->ReadBinary(uri, 1024U * 1024U * 1024U); }, source.unit_scale};
    ImportedScene scene;
    TRY_ASSIGN(scene, importer->Import(import_request));
    for (const auto& dependency : scene.dependencies) {
        if (dependency == source.source_uri)
            continue;
        std::vector<std::byte> bytes;
        TRY_ASSIGN(bytes, vfs_->ReadBinary(dependency, 1024U * 1024U * 1024U));
        TRY_VOID(context.AddSourceDependency(asset::AssetId::FromName(dependency.String()), Sha256(bytes)));
    }
    std::vector<asset::ProductDependency> dependencies;
    if (!scene.materials.empty() && resolve_) {
        std::vector<GeneratedImportedMaterial> generated;
        TRY_ASSIGN(generated, BuildImportedMaterials(scene, request.asset_id));
        for (const auto& material : generated) {
            asset::Product shader_product;
            TRY_ASSIGN(shader_product, resolve_(material.definition.shader));
            if (shader_product.type != kShaderProductType)
                return Err(ErrorCode::ValidationInvalidState, "generated material shader dependency is not a shader product");
            ShaderPayload shader;
            TRY_ASSIGN(shader, ParseShaderPayload(shader_product.payload));
            std::vector<asset::Product> texture_products;
            for (const auto& texture : material.textures) {
                ref<const asset::Vfs> texture_vfs = vfs_;
                std::optional<asset::AssetUri> image_uri;
                ref<asset::Vfs> memory_vfs;
                if (texture.source) {
                    image_uri = *texture.source;
                } else {
                    memory_vfs = createRef<asset::Vfs>();
                    auto mount = createRef<asset::MemoryMount>();
                    const auto path = *asset::AssetPath::Parse("mesh-generated/image/" + texture.id.String());
                    mount->Put(path, texture.embedded_bytes);
                    TRY_VOID(memory_vfs->MountAt("mesh-embedded-image", asset::AssetScheme::Engine, {}, 0, mount));
                    image_uri = asset::AssetUri::Engine(path);
                    texture_vfs = memory_vfs;
                }
                constexpr std::array semantic_names{"color", "normal", "data", "emissive", "occlusion", "depth", "environment"};
                config::Writer authored(false);
                authored.BeginObject();
                authored.Key("$schema");
                authored.String("https://schemas.woki.dev/texture/v1.schema.json");
                authored.Key("schema");
                authored.Unsigned(1);
                authored.Key("asset_id");
                authored.String(texture.id.String());
                authored.Key("source");
                authored.String(image_uri->String());
                authored.Key("semantic");
                authored.String(semantic_names[static_cast<size_t>(texture.semantic)]);
                authored.Key("color_space");
                authored.String(texture.color_space == TextureColorSpace::Srgb ? "srgb" : "linear");
                authored.Key("mips");
                authored.String("generate");
                authored.EndObject();
                const std::string descriptor = authored.Str();
                TextureBuilder builder(texture_vfs);
                asset::BuildContext texture_context;
                asset::Product product;
                TRY_ASSIGN(product, builder.Build({texture.id, *image_uri, Sha256(descriptor), request.target_fingerprint, request.revision}, texture_context, std::as_bytes(std::span(descriptor))));
                TRY_VOID(context.AddGeneratedProduct(product));
                // Keep generated image products as direct mesh dependencies.  The
                // material instance references them transitively, but cooked
                // manifests are allowed to prune transitive generated products.
                // Making the edge explicit guarantees that runtime texture
                // requests can resolve embedded images after packaging.
                dependencies.push_back({product.asset_id, product.product_hash});
                texture_products.push_back(std::move(product));
            }
            auto compiled = CompileMaterialType(material.definition, shader, shader_product.product_hash);
            if (!compiled.Valid())
                return Err(ErrorCode::ValidationInvalidState, compiled.diagnostics.empty() ? "generated material compilation failed" : compiled.diagnostics.front().message);
            std::vector definition_dependencies{asset::ProductDependency{shader_product.asset_id, shader_product.product_hash}};
            asset::Product definition;
            TRY_ASSIGN(definition, MakeMaterialDefinitionProduct(*compiled.definition, request.source_hash, std::move(definition_dependencies)));
            MaterialInstanceProduct instance;
            TRY_ASSIGN(instance, CompileMaterialInstance(material.instance, *compiled.definition, definition.product_hash));
            std::vector instance_dependencies{asset::ProductDependency{definition.asset_id, definition.product_hash}};
            for (const auto& texture : texture_products)
                instance_dependencies.push_back({texture.asset_id, texture.product_hash});
            asset::Product instance_product;
            TRY_ASSIGN(instance_product, MakeMaterialInstanceProduct(instance, request.source_hash, std::move(instance_dependencies)));
            TRY_VOID(context.AddGeneratedProduct(definition));
            TRY_VOID(context.AddGeneratedProduct(instance_product));
            dependencies.push_back({instance_product.asset_id, instance_product.product_hash});
        }
    }
    std::ranges::sort(dependencies);
    for (size_t index = 1; index < dependencies.size(); ++index)
        if (dependencies[index - 1].asset_id == dependencies[index].asset_id
            && dependencies[index - 1].product_hash != dependencies[index].product_hash)
            return Err(ErrorCode::ValidationInvalidState, "mesh dependencies contain conflicting products for one asset ID");
    dependencies.erase(
        std::ranges::unique(dependencies, {}, &asset::ProductDependency::asset_id).begin(),
        dependencies.end()
    );
    std::vector<std::byte> payload;
    TRY_ASSIGN(payload, BuildMeshProduct(scene, source, importer->Name(), dependencies));
    auto product = asset::MakeProduct(request.asset_id, kMeshProductType, kMeshProductVersion, descriptor_.version, request.source_hash, std::move(dependencies), request.target_fingerprint, std::move(payload));
    auto mesh = ParseMeshProduct(product.payload);
    if (!mesh)
        return Err(std::move(mesh).error());
    product.chunks.clear();
    const u64 header_size = mesh->lods.empty() ? product.payload.size() : mesh->lods.front().vertices.offset;
    product.chunks.push_back({asset::ProductChunkSemantic::Header, asset::ProductCompression::None, 1, 0, header_size, header_size, Sha256(std::span(product.payload).first(static_cast<size_t>(header_size)))});
    const auto append = [&](const MeshChunk& chunk, const asset::ProductChunkSemantic semantic) {
        product.chunks.push_back({semantic, asset::ProductCompression::None, 1, chunk.offset, chunk.size, chunk.size, chunk.checksum});
    };
    for (const auto& lod : mesh->lods) {
        append(lod.vertices, asset::ProductChunkSemantic::MeshVertices);
        append(lod.indices, asset::ProductChunkSemantic::MeshIndices);
        append(lod.meshlets, asset::ProductChunkSemantic::Meshlets);
    }
    for (const auto& image : mesh->images)
        append(image.payload, asset::ProductChunkSemantic::Generic);
    product.product_hash = asset::HashProduct(product);
    return Ok(std::move(product));
}

Result<void> RegisterMeshAssetBuilder(asset::AssetServices& services, ref<const asset::Vfs> vfs, ref<const ModelImporterRegistry> importers, MeshBuilder::ResolveProduct resolve) {
    if (services.builders == nullptr)
        return Err(ErrorCode::InvalidState, "asset services has no builder registry");
    return services.builders->Register(createRef<const MeshBuilder>(std::move(vfs), std::move(importers), std::move(resolve)));
}

} // namespace woki::gfx
