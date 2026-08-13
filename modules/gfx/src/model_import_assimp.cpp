#include <algorithm>
#include <cctype>
#include <charconv>
#include <cstring>
#include <map>
#include <numeric>
#include <set>

#include <assimp/Importer.hpp>
#include <assimp/IOStream.hpp>
#include <assimp/IOSystem.hpp>
#include <assimp/material.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include <woki/gfx/advanced/model_import.hpp>

namespace woki::gfx {
namespace {

math::mat4f Matrix(const aiMatrix4x4& value) {
    // Assimp matrices are copied by row into Woki's column-vector convention.
    // The adapters store quaternions as (x,y,z,w).
    return math::mat4f(
        math::layout::rowm,
        value.a1,
        value.a2,
        value.a3,
        value.a4,
        value.b1,
        value.b2,
        value.b3,
        value.b4,
        value.c1,
        value.c2,
        value.c3,
        value.c4,
        value.d1,
        value.d2,
        value.d3,
        value.d4
    );
}

math::vec3f Vector(const aiVector3D& value) {
    return {value.x, value.y, value.z};
}

math::vec4f Quaternion(const aiQuaternion& value) {
    return {value.x, value.y, value.z, value.w};
}

std::string Lower(std::string value) {
    std::ranges::transform(value, value.begin(), [](const unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return value;
}

class VfsStream final : public Assimp::IOStream {
public:
    explicit VfsStream(std::vector<std::byte> bytes)
        : bytes_(std::move(bytes)) {}

    size_t Read(void* buffer, const size_t size, size_t count) override {
        if (size == 0 || count > (bytes_.size() - std::min(offset_, bytes_.size())) / size)
            count = size == 0 ? 0 : (bytes_.size() - std::min(offset_, bytes_.size())) / size;
        const size_t bytes = size * count;
        if (bytes != 0)
            std::memcpy(buffer, bytes_.data() + offset_, bytes);
        offset_ += bytes;
        return count;
    }

    size_t Write(const void*, size_t, size_t) override {
        return 0;
    }

    aiReturn Seek(const size_t offset, const aiOrigin origin) override {
        const size_t base = origin == aiOrigin_SET ? 0 : origin == aiOrigin_CUR ? offset_ : bytes_.size();
        if (offset > bytes_.size() - std::min(base, bytes_.size()))
            return aiReturn_FAILURE;
        offset_ = base + offset;
        return aiReturn_SUCCESS;
    }

    size_t Tell() const override {
        return offset_;
    }

    size_t FileSize() const override {
        return bytes_.size();
    }

    void Flush() override {}

private:
    std::vector<std::byte> bytes_;
    size_t offset_{};
};

class VfsIoSystem final : public Assimp::IOSystem {
public:
    VfsIoSystem(const ModelImportRequest& request, ImportedScene& scene)
        : request_(request),
          scene_(scene) {}

    bool Exists(const char* path) const override {
        return Resolve(path).has_value();
    }

    char getOsSeparator() const override {
        return '/';
    }

    Assimp::IOStream* Open(const char* path, const char* mode) override {
        if (mode == nullptr || mode[0] != 'r')
            return nullptr;
        auto resolved = Resolve(path);
        if (!resolved)
            return nullptr;
        if (*resolved == request_.source_uri)
            return new VfsStream(std::vector<std::byte>(request_.bytes.begin(), request_.bytes.end()));
        auto bytes = request_.resolve(*resolved);
        if (!bytes) {
            scene_.diagnostics.push_back(
                {DiagnosticSeverity::Error,
                    "IMP_EXTERNAL_READ",
                    "unable to resolve Assimp dependency '" + resolved->String()
                        + "': " + std::string(bytes.error().Message())}
            );
            return nullptr;
        }
        scene_.dependencies.push_back(*resolved);
        return new VfsStream(std::move(*bytes));
    }

    void Close(Assimp::IOStream* stream) override {
        delete stream;
    }

private:
    std::optional<asset::AssetUri> Resolve(const std::string_view path) const {
        if (path == request_.source_uri.String())
            return request_.source_uri;
        if (auto absolute = asset::AssetUri::Parse(path); absolute)
            return *absolute;
        const auto slash = request_.source_uri.String().find_last_of('/');
        auto relative = asset::AssetUri::Parse(
            std::string(request_.source_uri.String().substr(0, slash + 1)) + std::string(path)
        );
        return relative ? std::optional<asset::AssetUri>(*relative) : std::nullopt;
    }

    const ModelImportRequest& request_;
    ImportedScene& scene_;
};

void AddTexture(
    const aiMaterial& source,
    const aiTextureType type,
    std::string semantic,
    ImportedMaterial& destination,
    ImportedScene& scene,
    const asset::AssetUri& source_uri
) {
    for (u32 index = 0; index < source.GetTextureCount(type); ++index) {
        aiString path;
        if (source.GetTexture(type, index, &path) != aiReturn_SUCCESS)
            continue;
        const std::string raw = path.C_Str();
        if (raw.empty())
            continue;
        u32 image_index{};
        if (!raw.empty() && raw.front() == '*') {
            const auto parsed = std::from_chars(raw.data() + 1, raw.data() + raw.size(), image_index);
            if (parsed.ec != std::errc{} || image_index >= scene.images.size())
                continue;
        } else if (!raw.empty()) {
            const auto embedded = std::ranges::find_if(scene.images, [&](const ImportedImage& image) {
                const auto image_name = image.uri.substr(image.uri.find_last_of("/\\") + 1);
                const auto raw_name = raw.substr(raw.find_last_of("/\\") + 1);
                return image.embedded && (image.uri == raw || image.uri.ends_with(raw) || raw.ends_with(image.uri)
                                          || image_name == raw_name);
            });
            if (embedded != scene.images.end()) {
                image_index = static_cast<u32>(embedded - scene.images.begin());
            } else {
                const auto unnamed_embedded = std::ranges::find_if(scene.images, [](const ImportedImage& image) {
                    return image.embedded && !image.payload.empty();
                });
                if (unnamed_embedded != scene.images.end()) {
                    image_index = static_cast<u32>(unnamed_embedded - scene.images.begin());
                    destination.textures.push_back({semantic, raw, image_index});
                    continue;
                }
                const auto slash = source_uri.String().find_last_of('/');
                auto uri = asset::AssetUri::Parse(source_uri.String().substr(0, slash + 1) + raw);
                if (uri) {
                    scene.dependencies.push_back(*uri);
                    const auto existing = std::ranges::find(scene.images, raw, &ImportedImage::uri);
                    if (existing == scene.images.end()) {
                        image_index = static_cast<u32>(scene.images.size());
                        scene.images.push_back({.uri = raw});
                    } else
                        image_index = static_cast<u32>(existing - scene.images.begin());
                }
            }
        }
        destination.textures.push_back({semantic, raw, image_index});
    }
}

class AssimpModelImporter final : public ModelImporter {
public:
    [[nodiscard]] std::string_view Name() const noexcept override {
        return "assimp";
    }

    [[nodiscard]] bool Supports(const std::string_view extension, const std::string_view) const noexcept override {
        static constexpr std::array formats{"fbx", "obj", "dae", "3ds", "blend", "ply", "stl", "x"};
        std::string normalized(extension);
        if (!normalized.empty() && normalized.front() == '.')
            normalized.erase(normalized.begin());
        normalized = Lower(std::move(normalized));
        return std::ranges::find(formats, normalized) != formats.end();
    }

    [[nodiscard]] Result<ImportedScene> Import(const ModelImportRequest& request) const override {
        std::string hint;
        const auto dot = request.source_uri.Path().String().find_last_of('.');
        if (dot != std::string::npos)
            hint = request.source_uri.Path().String().substr(dot + 1);
        if (!hint.empty() && hint.front() == '.')
            hint.erase(hint.begin());
        ImportedScene result;
        result.name = request.source_uri.Path().String();
        result.conversion = {.source_coordinate_system = "Assimp right-handed source basis",
            .source_units_per_meter = 1.0F,
            .applied_scale = request.unit_scale};
        result.dependencies.push_back(request.source_uri);
        Assimp::Importer importer;
        importer.SetIOHandler(new VfsIoSystem(request, result));
        constexpr unsigned flags = aiProcess_Triangulate | aiProcess_JoinIdenticalVertices | aiProcess_SortByPType
                                   | aiProcess_ValidateDataStructure | aiProcess_ImproveCacheLocality
                                   | aiProcess_FindInvalidData | aiProcess_LimitBoneWeights;
        const aiScene* source = importer.ReadFile(request.source_uri.String(), flags);
        if (source == nullptr)
            return Err(
                ErrorCode::ParseInvalidFormat,
                std::string("Assimp import failed: ") + importer.GetErrorString()
            );

        for (u32 index = 0; index < source->mNumTextures; ++index) {
            const aiTexture& texture = *source->mTextures[index];
            ImportedImage image{.uri = texture.mFilename.length > 0
                                         ? texture.mFilename.C_Str()
                                         : "*" + std::to_string(index),
                .mime_type = texture.achFormatHint,
                .embedded = true};
            const size_t size = texture.mHeight == 0
                                    ? texture.mWidth
                                    : static_cast<size_t>(texture.mWidth) * texture.mHeight * sizeof(aiTexel);
            image.payload.resize(size);
            if (size != 0)
                std::memcpy(image.payload.data(), texture.pcData, size);
            result.images.push_back(std::move(image));
        }

        std::map<const aiNode*, u32> node_indices;
        std::map<std::string, std::vector<u32>, std::less<>> nodes_by_name;
        const auto add_node =
            [&](const auto& self, const aiNode* node, const i32 parent, const math::mat4f& parent_world) -> void {
            const u32 index = static_cast<u32>(result.nodes.size());
            node_indices.emplace(node, index);
            math::mat4f local = Matrix(node->mTransformation);
            local(0, 3) *= request.unit_scale;
            local(1, 3) *= request.unit_scale;
            local(2, 3) *= request.unit_scale;
            ImportedNode converted{.name = node->mName.C_Str(),
                .parent = parent,
                .local = local,
                .world = parent_world * local};
            converted.meshes.assign(node->mMeshes, node->mMeshes + node->mNumMeshes);
            result.nodes.push_back(std::move(converted));
            nodes_by_name[result.nodes.back().name].push_back(index);
            if (parent >= 0)
                result.nodes[static_cast<u32>(parent)].children.push_back(index);
            for (u32 child = 0; child < node->mNumChildren; ++child)
                self(self, node->mChildren[child], static_cast<i32>(index), result.nodes[index].world);
        };
        add_node(add_node, source->mRootNode, -1, math::mat4f::identity());

        // Assimp keeps mesh vertices in the mesh node's local space.  Woki's
        // cooked mesh is submitted as one drawable object, so preserve the
        // node placement at import time instead of silently dropping it in
        // the renderer.  This is especially important for FBX files whose
        // body parts are represented by separate mesh nodes.
        std::vector<math::mat4f> mesh_transforms(source->mNumMeshes, math::mat4f::identity());
        for (const auto& node : result.nodes)
            for (const u32 mesh_index : node.meshes)
                if (mesh_index < mesh_transforms.size())
                    mesh_transforms[mesh_index] = node.world;

        result.materials.reserve(source->mNumMaterials);
        for (u32 index = 0; index < source->mNumMaterials; ++index) {
            const aiMaterial& material = *source->mMaterials[index];
            ImportedMaterial converted;
            aiString name;
            if (material.Get(AI_MATKEY_NAME, name) == aiReturn_SUCCESS)
                converted.name = name.C_Str();
            aiColor4D color;
            if (material.Get(AI_MATKEY_BASE_COLOR, color) == aiReturn_SUCCESS
                || material.Get(AI_MATKEY_COLOR_DIFFUSE, color) == aiReturn_SUCCESS)
                converted.base_color = {color.r, color.g, color.b, color.a};
            aiColor3D emissive;
            if (material.Get(AI_MATKEY_COLOR_EMISSIVE, emissive) == aiReturn_SUCCESS)
                converted.emissive = {emissive.r, emissive.g, emissive.b};
            static_cast<void>(material.Get(AI_MATKEY_METALLIC_FACTOR, converted.metallic));
            static_cast<void>(material.Get(AI_MATKEY_ROUGHNESS_FACTOR, converted.roughness));
            static_cast<void>(material.Get(AI_MATKEY_OPACITY, converted.opacity));
            int two_sided{};
            if (material.Get(AI_MATKEY_TWOSIDED, two_sided) == aiReturn_SUCCESS)
                converted.double_sided = two_sided != 0;
            converted.alpha_mode = converted.opacity < 1.0F ? ImportedAlphaMode::Blend : ImportedAlphaMode::Opaque;
            AddTexture(material, aiTextureType_BASE_COLOR, "base_color", converted, result, request.source_uri);
            AddTexture(material, aiTextureType_DIFFUSE, "base_color", converted, result, request.source_uri);
            AddTexture(material, aiTextureType_NORMALS, "normal", converted, result, request.source_uri);
            AddTexture(material, aiTextureType_METALNESS, "metallic", converted, result, request.source_uri);
            AddTexture(material, aiTextureType_DIFFUSE_ROUGHNESS, "roughness", converted, result, request.source_uri);
            AddTexture(material, aiTextureType_EMISSIVE, "emissive", converted, result, request.source_uri);
            result.materials.push_back(std::move(converted));
        }

        std::map<std::string, u32, std::less<>> joints_by_name;
        ImportedSkeleton skeleton{.name = result.name};
        for (u32 mesh_index = 0; mesh_index < source->mNumMeshes; ++mesh_index) {
            const aiMesh& mesh = *source->mMeshes[mesh_index];
            for (u32 bone_index = 0; bone_index < mesh.mNumBones; ++bone_index) {
                const aiBone& bone = *mesh.mBones[bone_index];
                const std::string name = bone.mName.C_Str();
                if (joints_by_name.contains(name))
                    continue;
                const auto node = nodes_by_name.find(name);
                if (node == nodes_by_name.end()) {
                    result.diagnostics.push_back(
                        {DiagnosticSeverity::Warning,
                            "IMP_MISSING_BONE_NODE",
                            "bone '" + name + "' has no matching node"}
                    );
                    continue;
                }
                if (node->second.size() > 1)
                    return Err(
                        ErrorCode::ValidationInvalidState,
                        "Assimp bone name '" + name + "' matches duplicate nodes"
                    );
                joints_by_name.emplace(name, static_cast<u32>(skeleton.joints.size()));
                math::mat4f inverse_bind = Matrix(bone.mOffsetMatrix);
                // Unit conversion is C*M*C^-1 with C=uniform scale. This only
                // changes inverse-bind translation; rotations remain unchanged.
                inverse_bind(0, 3) *= request.unit_scale;
                inverse_bind(1, 3) *= request.unit_scale;
                inverse_bind(2, 3) *= request.unit_scale;
                skeleton.joints.push_back({name, node->second.front(), -1, inverse_bind});
            }
        }
        for (auto& joint : skeleton.joints) {
            i32 parent = result.nodes[joint.node].parent;
            while (parent >= 0) {
                const auto found = joints_by_name.find(result.nodes[static_cast<u32>(parent)].name);
                if (found != joints_by_name.end()) {
                    joint.parent = static_cast<i32>(found->second);
                    break;
                }
                parent = result.nodes[static_cast<u32>(parent)].parent;
            }
        }
        if (!skeleton.joints.empty()) {
            result.skeletons.push_back(std::move(skeleton));
            ImportedSkin skin{.name = result.name, .skeleton = 0};
            skin.joints.resize(result.skeletons.front().joints.size());
            std::iota(skin.joints.begin(), skin.joints.end(), 0U);
            result.skins.push_back(std::move(skin));
        }

        result.meshes.reserve(source->mNumMeshes);
        for (u32 mesh_index = 0; mesh_index < source->mNumMeshes; ++mesh_index) {
            const aiMesh& mesh = *source->mMeshes[mesh_index];
            ImportedMesh converted_mesh{.name = mesh.mName.C_Str()};
            if (mesh.HasBones())
                converted_mesh.skin = 0;
            ImportedPrimitive primitive{.name = converted_mesh.name,
                .topology = ImportedTopology::Triangles,
                .material_slot = mesh.mMaterialIndex};
            const math::mat4f& mesh_transform = mesh_transforms[mesh_index];
            primitive.positions.reserve(mesh.mNumVertices);
            if (mesh.HasNormals())
                primitive.normals.reserve(mesh.mNumVertices);
            if (mesh.HasTangentsAndBitangents())
                primitive.tangents.reserve(mesh.mNumVertices);
            if (mesh.HasTextureCoords(0))
                primitive.uv0.reserve(mesh.mNumVertices);
            if (mesh.HasTextureCoords(1))
                primitive.uv1.reserve(mesh.mNumVertices);
            if (mesh.HasVertexColors(0))
                primitive.colors.reserve(mesh.mNumVertices);
            if (mesh.HasBones())
                primitive.influences.resize(mesh.mNumVertices);
            for (u32 vertex = 0; vertex < mesh.mNumVertices; ++vertex) {
                const math::vec3f source_position = Vector(mesh.mVertices[vertex]) * request.unit_scale;
                const math::vec4f position = mesh_transform * math::vec4f{source_position, 1.0F};
                primitive.positions.push_back({position.x, position.y, position.z});
                if (mesh.HasNormals())
                {
                    const math::vec3f source_normal = Vector(mesh.mNormals[vertex]);
                    const math::vec4f normal = mesh_transform * math::vec4f{source_normal, 0.0F};
                    primitive.normals.push_back(math::vec3f{normal.x, normal.y, normal.z}.normalized());
                }
                if (mesh.HasTangentsAndBitangents())
                {
                    const math::vec3f source_tangent = Vector(mesh.mTangents[vertex]);
                    const math::vec4f tangent = mesh_transform * math::vec4f{source_tangent, 0.0F};
                    primitive.tangents.push_back(
                        {tangent.x, tangent.y, tangent.z, 1.0F}
                    );
                }
                if (mesh.HasTextureCoords(0))
                    primitive.uv0.push_back({mesh.mTextureCoords[0][vertex].x, mesh.mTextureCoords[0][vertex].y});
                if (mesh.HasTextureCoords(1))
                    primitive.uv1.push_back({mesh.mTextureCoords[1][vertex].x, mesh.mTextureCoords[1][vertex].y});
                if (mesh.HasVertexColors(0))
                    primitive.colors.push_back(
                        {mesh.mColors[0][vertex].r,
                            mesh.mColors[0][vertex].g,
                            mesh.mColors[0][vertex].b,
                            mesh.mColors[0][vertex].a}
                    );
            }
            for (u32 face = 0; face < mesh.mNumFaces; ++face) {
                if (mesh.mFaces[face].mNumIndices != 3)
                    return Err(ErrorCode::GraphicsUnsupportedApi, "Assimp produced a non-triangle face");
                primitive.indices
                    .insert(primitive.indices.end(), mesh.mFaces[face].mIndices, mesh.mFaces[face].mIndices + 3);
            }
            for (u32 bone_index = 0; bone_index < mesh.mNumBones; ++bone_index) {
                const aiBone& bone = *mesh.mBones[bone_index];
                const auto joint = joints_by_name.find(bone.mName.C_Str());
                if (joint == joints_by_name.end())
                    continue;
                for (u32 weight = 0; weight < bone.mNumWeights; ++weight) {
                    const auto value = bone.mWeights[weight];
                    if (value.mVertexId >= primitive.influences.size())
                        return Err(ErrorCode::ValidationOutOfRange, "Assimp bone weight vertex is out of range");
                    auto& slots = primitive.influences[value.mVertexId];
                    const auto empty = std::ranges::find(slots, 0.0F, &ImportedInfluence::weight);
                    if (empty != slots.end())
                        *empty = {joint->second, value.mWeight};
                    else {
                        const auto smallest = std::ranges::min_element(slots, {}, &ImportedInfluence::weight);
                        if (smallest->weight < value.mWeight)
                            *smallest = {joint->second, value.mWeight};
                    }
                }
            }
            if (!primitive.positions.empty()) {
                primitive.bounds_min = primitive.bounds_max = primitive.positions.front();
                for (const auto& position : primitive.positions)
                    for (u32 axis = 0; axis < 3; ++axis) {
                        primitive.bounds_min[axis] = std::min(primitive.bounds_min[axis], position[axis]);
                        primitive.bounds_max[axis] = std::max(primitive.bounds_max[axis], position[axis]);
                    }
            }
            converted_mesh.primitives.push_back(std::move(primitive));
            result.meshes.push_back(std::move(converted_mesh));
        }

        constexpr f64 kDefaultTicksPerSecond = 25.0;
        for (u32 animation_index = 0; animation_index < source->mNumAnimations; ++animation_index) {
            const aiAnimation& animation = *source->mAnimations[animation_index];
            const f64 ticks = animation.mTicksPerSecond > 1.0e-8 ? animation.mTicksPerSecond : kDefaultTicksPerSecond;
            ImportedAnimationClip clip{.name = animation.mName.length > 0
                                                   ? animation.mName.C_Str()
                                                   : "animation_" + std::to_string(animation_index),
                .duration = static_cast<f32>(animation.mDuration / ticks),
                .source_ticks_per_second = static_cast<f32>(ticks)};
            for (u32 channel_index = 0; channel_index < animation.mNumChannels; ++channel_index) {
                const aiNodeAnim& channel = *animation.mChannels[channel_index];
                const auto node = nodes_by_name.find(channel.mNodeName.C_Str());
                if (node == nodes_by_name.end()) {
                    result.diagnostics.push_back(
                        {DiagnosticSeverity::Warning,
                            "IMP_MISSING_ANIMATION_NODE",
                            "animation channel node '" + std::string(channel.mNodeName.C_Str()) + "' was not found"}
                    );
                    continue;
                }
                const auto append =
                    [&](const AnimationPath path, const u32 count, const auto* keys, const auto convert) {
                        if (count == 0)
                            return;
                        ImportedAnimationChannel output{.node = node->second.front(), .path = path};
                        output.times.reserve(count);
                        output.values.reserve(count);
                        for (u32 key = 0; key < count; ++key) {
                            output.times.push_back(static_cast<f32>(keys[key].mTime / ticks));
                            output.values.push_back(convert(keys[key].mValue));
                        }
                        clip.channels.push_back(std::move(output));
                    };
                append(
                    AnimationPath::Translation,
                    channel.mNumPositionKeys,
                    channel.mPositionKeys,
                    [&](const aiVector3D& value) {
                        return math::vec4f{value.x * request.unit_scale,
                            value.y * request.unit_scale,
                            value.z * request.unit_scale,
                            0.0F};
                    }
                );
                append(AnimationPath::Rotation, channel.mNumRotationKeys, channel.mRotationKeys, Quaternion);
                append(
                    AnimationPath::Scale,
                    channel.mNumScalingKeys,
                    channel.mScalingKeys,
                    [](const aiVector3D& value) { return math::vec4f{value.x, value.y, value.z, 0.0F}; }
                );
            }
            result.animations.push_back(std::move(clip));
        }
        TRY_VOID(NormalizeInfluences(result));
        return Ok(std::move(result));
    }
};
} // namespace

ref<const ModelImporter> CreateAssimpModelImporter() {
    return createRef<const AssimpModelImporter>();
}

} // namespace woki::gfx
