#include <algorithm>
#include <cmath>
#include <map>
#include <numeric>
#include <set>

#include <woki/gfx/advanced/model_import.hpp>

namespace woki::gfx {

Result<std::vector<GeneratedImportedMaterial>> BuildImportedMaterials(const ImportedScene& scene, const asset::AssetId model_id) {
    std::vector<GeneratedImportedMaterial> output;
    output.reserve(scene.materials.size());
    const auto property = [](const std::string_view name) { return MaterialPropertyId::FromName(name); };
    const auto semantic = [](const std::string_view name) {
        if (name == "normal")
            return TextureSemantic::Normal;
        if (name == "emissive")
            return TextureSemantic::Emissive;
        if (name == "occlusion")
            return TextureSemantic::Occlusion;
        if (name == "metallic_roughness" || name == "metallic" || name == "roughness")
            return TextureSemantic::Data;
        return TextureSemantic::Color;
    };
    for (u32 slot = 0; slot < scene.materials.size(); ++slot) {
        const auto& imported = scene.materials[slot];
        const std::string identity = model_id.String() + "/material/" + std::to_string(slot);
        GeneratedImportedMaterial generated;
        generated.slot = slot;
        generated.definition.id = asset::AssetId::FromName(identity + "/type");
        generated.definition.name = imported.name.empty() ? "Imported material " + std::to_string(slot) : imported.name;
        TRY_ASSIGN(generated.definition.shader, asset::AssetId::Parse("85eb4828-6169-5ce0-aeae-0d1b00ce22dc"));
        generated.definition.product_family = "generated-imported-pbr";
        generated.definition.passes = {MaterialPass::Forward, MaterialPass::Velocity};
        generated.definition.entry_points = {{MaterialPass::Forward, {"pbr_static_vs", "pbr_fs"}}, {MaterialPass::Velocity, {"pbr_velocity_static_vs", "pbr_velocity_fs"}}};
        generated.definition.render_state.blend = imported.alpha_mode == ImportedAlphaMode::Blend ? MaterialBlendMode::Alpha : MaterialBlendMode::Opaque;
        generated.definition.render_state.cull = imported.double_sided ? rhi::CullMode::None : rhi::CullMode::Back;
        generated.definition.render_state.depth_write = imported.alpha_mode != ImportedAlphaMode::Blend;
        generated.definition.render_state.alpha_test = imported.alpha_mode == ImportedAlphaMode::Mask;
        generated.definition.properties = {
            {.id = property("base_color_factor"),
                .name = "base_color_factor",
                .type = MaterialValueType::Vec4,
                .default_value = std::array{imported.base_color.x, imported.base_color.y, imported.base_color.z, imported.base_color.w * imported.opacity},
                .offset = std::nullopt},
            {.id = property("emissive_factor"),
                .name = "emissive_factor",
                .type = MaterialValueType::Vec3,
                .default_value = std::array{imported.emissive.x, imported.emissive.y, imported.emissive.z},
                .offset = std::nullopt},
            {.id = property("normal_scale"), .name = "normal_scale", .type = MaterialValueType::F32, .default_value = imported.normal_scale, .offset = std::nullopt},
            {.id = property("metallic_factor"), .name = "metallic_factor", .type = MaterialValueType::F32, .default_value = imported.metallic, .offset = std::nullopt},
            {.id = property("roughness_factor"), .name = "roughness_factor", .type = MaterialValueType::F32, .default_value = imported.roughness, .offset = std::nullopt},
            {.id = property("occlusion_strength"), .name = "occlusion_strength", .type = MaterialValueType::F32, .default_value = imported.occlusion_strength, .offset = std::nullopt},
            {.id = property("alpha_cutoff"), .name = "alpha_cutoff", .type = MaterialValueType::F32, .default_value = imported.alpha_cutoff, .offset = std::nullopt},
        };
        generated.instance.id = asset::AssetId::FromName(identity + "/instance");
        generated.instance.type = generated.definition.id;

        struct TextureSlot {
            std::string_view name;
            u32 binding;
            TextureSemantic semantic;
        };

        constexpr std::array slots{
            TextureSlot{"base_color_texture", 1, TextureSemantic::Color},
            TextureSlot{"normal_texture", 3, TextureSemantic::Normal},
            TextureSlot{"metallic_roughness_texture", 4, TextureSemantic::Data},
            TextureSlot{"emissive_texture", 5, TextureSemantic::Emissive},
            TextureSlot{"occlusion_texture", 6, TextureSemantic::Occlusion},
        };
        for (const auto& target : slots) {
            const auto imported_texture = std::ranges::find_if(imported.textures, [&](const ImportedTextureRef& value) {
                if (target.semantic == TextureSemantic::Data)
                    return value.semantic == "metallic_roughness" || value.semantic == "metallic" || value.semantic == "roughness";
                return semantic(value.semantic) == target.semantic;
            });
            asset::AssetId texture_id = BuiltinFallbackTextureId(target.semantic);
            if (imported_texture != imported.textures.end() && imported_texture->image < scene.images.size()) {
                const auto& image = scene.images[imported_texture->image];
                texture_id = asset::AssetId::FromName(identity + "/image/" + std::to_string(imported_texture->image) + "/" + target.name.data());
                GeneratedImportedTexture texture{.id = texture_id,
                    .semantic = target.semantic,
                    .color_space = target.semantic == TextureSemantic::Color || target.semantic == TextureSemantic::Emissive ? TextureColorSpace::Srgb : TextureColorSpace::Linear,
                    .source = std::nullopt,
                    .mime_type = image.mime_type,
                    .embedded_bytes = image.payload};
                if (!image.embedded)
                    for (const auto& dependency : scene.dependencies)
                        if (dependency.String().ends_with(image.uri)) {
                            texture.source = dependency;
                            generated.dependencies.push_back(dependency);
                            break;
                        }
                generated.textures.push_back(std::move(texture));
            }
            generated.definition.textures.push_back({property(target.name), std::string(target.name), MaterialValueType::Texture2D, target.binding, target.semantic, BuiltinFallbackTextureId(target.semantic)});
            generated.instance.overrides[property(target.name)] = texture_id;
        }
        generated.definition.samplers.push_back({property("material_sampler"), "material_sampler", 2, {}});
        std::ranges::sort(generated.dependencies, {}, [](const asset::AssetUri& uri) { return uri.String(); });
        generated.dependencies.erase(std::unique(generated.dependencies.begin(), generated.dependencies.end()), generated.dependencies.end());
        output.push_back(std::move(generated));
    }
    return Ok(std::move(output));
}

Result<void> ModelImporterRegistry::Register(ref<const ModelImporter> importer) {
    if (importer == nullptr || importer->Name().empty())
        return Err(ErrorCode::ValidationInvalidState, "model importer is invalid");
    if (std::ranges::any_of(importers_, [&](const auto& current) { return current->Name() == importer->Name(); }))
        return Err(ErrorCode::ValidationInvalidState, "model importer name is already registered");
    importers_.push_back(std::move(importer));
    return Ok();
}

const ModelImporter* ModelImporterRegistry::Select(const std::string_view extension, const std::string_view mime_type, const std::string_view preferred) const noexcept {
    if (preferred != "auto") {
        const auto named = std::ranges::find_if(importers_, [&](const auto& importer) { return importer->Name() == preferred; });
        return named != importers_.end() && (*named)->Supports(extension, mime_type) ? named->get() : nullptr;
    }
    const auto found = std::ranges::find_if(importers_, [&](const auto& importer) { return importer->Supports(extension, mime_type); });
    return found == importers_.end() ? nullptr : found->get();
}

Result<void> NormalizeInfluences(ImportedScene& scene) {
    for (const auto& clip : scene.animations) {
        if (!std::isfinite(clip.duration) || clip.duration < 0.0F)
            return Err(ErrorCode::ValidationOutOfRange, "animation duration is invalid");
        for (const auto& channel : clip.channels) {
            if (channel.node >= scene.nodes.size())
                return Err(ErrorCode::ValidationOutOfRange, "animation channel node is out of range");
            if (channel.interpolation == AnimationInterpolation::CubicSpline)
                return Err(ErrorCode::GraphicsUnsupportedApi, "cubic spline animation requires tangent serialization and evaluation");
            if (channel.times.empty() || channel.times.size() != channel.values.size() || !std::ranges::is_sorted(channel.times) || std::adjacent_find(channel.times.begin(), channel.times.end()) != channel.times.end())
                return Err(ErrorCode::ValidationInvalidState, "animation keys must have matching values and unique sorted times");
            for (const f32 time : channel.times)
                if (!std::isfinite(time) || time < 0.0F || time > clip.duration)
                    return Err(ErrorCode::ValidationOutOfRange, "animation key time is outside clip duration");
            for (const auto& value : channel.values)
                for (u32 component = 0; component < 4; ++component)
                    if (!std::isfinite(value[component]))
                        return Err(ErrorCode::ValidationInvalidState, "animation key value is not finite");
        }
    }
    std::vector<std::vector<u32>> remaps(scene.skeletons.size());
    for (u32 skeleton_index = 0; skeleton_index < scene.skeletons.size(); ++skeleton_index) {
        auto& skeleton = scene.skeletons[skeleton_index];
        std::set<std::string, std::less<>> names;
        std::vector<u8> state(skeleton.joints.size());
        std::vector<u32> order;
        const auto visit = [&](const auto& self, const u32 index) -> Result<void> {
            if (index >= skeleton.joints.size())
                return Err(ErrorCode::ValidationOutOfRange, "skeleton parent is out of range");
            if (state[index] == 1)
                return Err(ErrorCode::ValidationInvalidState, "skeleton contains a parent cycle");
            if (state[index] == 2)
                return Ok();
            state[index] = 1;
            const i32 parent = skeleton.joints[index].parent;
            if (parent < -1)
                return Err(ErrorCode::ValidationOutOfRange, "skeleton parent is invalid");
            if (parent >= 0)
                TRY_VOID(self(self, static_cast<u32>(parent)));
            state[index] = 2;
            order.push_back(index);
            return Ok();
        };
        for (u32 index = 0; index < skeleton.joints.size(); ++index) {
            if (skeleton.joints[index].name.empty() || !names.insert(skeleton.joints[index].name).second)
                return Err(ErrorCode::ValidationInvalidState, "skeleton contains an empty or duplicate bone name");
            if (skeleton.joints[index].node >= scene.nodes.size())
                return Err(ErrorCode::ValidationOutOfRange, "skeleton joint node is out of range");
            TRY_VOID(visit(visit, index));
        }
        auto& remap = remaps[skeleton_index];
        remap.resize(order.size());
        for (u32 index = 0; index < order.size(); ++index)
            remap[order[index]] = index;
        std::vector<ImportedJoint> normalized;
        normalized.reserve(order.size());
        for (const u32 old : order) {
            auto joint = skeleton.joints[old];
            if (joint.parent >= 0)
                joint.parent = static_cast<i32>(remap[static_cast<u32>(joint.parent)]);
            normalized.push_back(std::move(joint));
        }
        skeleton.joints = std::move(normalized);
    }
    for (auto& skin : scene.skins) {
        if (skin.skeleton >= scene.skeletons.size())
            return Err(ErrorCode::ValidationOutOfRange, "skin skeleton is out of range");
        for (auto& joint : skin.joints) {
            if (joint >= remaps[skin.skeleton].size())
                return Err(ErrorCode::ValidationOutOfRange, "skin joint remap is out of range");
            joint = remaps[skin.skeleton][joint];
        }
    }
    for (auto& mesh : scene.meshes) {
        u32 joint_count{};
        if (mesh.skin) {
            if (*mesh.skin >= scene.skins.size() || scene.skins[*mesh.skin].skeleton >= scene.skeletons.size())
                return Err(ErrorCode::ValidationOutOfRange, "mesh references an out-of-range skin or skeleton");
            joint_count = static_cast<u32>(scene.skins[*mesh.skin].joints.size());
        }
        for (auto& primitive : mesh.primitives) {
            if (!primitive.influences.empty() && !mesh.skin)
                return Err(ErrorCode::ValidationInvalidState, "skinned primitive has no mesh-specific skin association");
            for (auto& influences : primitive.influences) {
                std::map<u32, f32> merged;
                for (const auto influence : influences) {
                    if (influence.weight <= 0.0F)
                        continue;
                    if (influence.joint >= joint_count)
                        return Err(ErrorCode::ValidationOutOfRange, "vertex influence references an out-of-range joint");
                    merged[scene.skins[*mesh.skin].joints[influence.joint]] += influence.weight;
                }
                std::vector<ImportedInfluence> sorted;
                for (const auto [joint, weight] : merged)
                    sorted.push_back({joint, weight});
                std::ranges::sort(sorted, [](const auto& left, const auto& right) { return left.weight != right.weight ? left.weight > right.weight : left.joint < right.joint; });
                influences = {};
                const size_t count = std::min(influences.size(), sorted.size());
                f32 sum{};
                for (size_t index = 0; index < count; ++index) {
                    influences[index] = sorted[index];
                    sum += sorted[index].weight;
                }
                if (sum <= 1.0e-8F) {
                    influences[0] = {0, 1.0F};
                } else {
                    for (size_t index = 0; index < count; ++index)
                        influences[index].weight /= sum;
                }
            }
        }
    }
    for (auto& skin : scene.skins) {
        skin.joints.resize(scene.skeletons[skin.skeleton].joints.size());
        std::iota(skin.joints.begin(), skin.joints.end(), 0U);
    }
    return Ok();
}

#ifndef __EMSCRIPTEN__
Result<ModelImporterRegistry> CreateDefaultModelImporterRegistry() {
    ModelImporterRegistry registry;
    TRY_VOID(registry.Register(CreateFastGltfModelImporter()));
    TRY_VOID(registry.Register(CreateAssimpModelImporter()));
    return Ok(std::move(registry));
}
#endif

} // namespace woki::gfx
