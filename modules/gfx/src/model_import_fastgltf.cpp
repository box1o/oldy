#include <algorithm>
#include <numeric>

#include <fastgltf/core.hpp>
#include <fastgltf/tools.hpp>

#include <woki/gfx/advanced/model_import.hpp>

namespace woki::gfx {
namespace {

template <size_t N>
math::vec<N, f32> Vector(const fastgltf::math::vec<f32, N>& value) {
    math::vec<N, f32> result;
    for (size_t index = 0; index < N; ++index)
        result[index] = value[index];
    return result;
}

math::mat4f Matrix(const fastgltf::math::fmat4x4& value) {
    math::mat4f result;
    for (size_t column = 0; column < 4; ++column)
        for (size_t row = 0; row < 4; ++row)
            result(row, column) = value[column][row];
    return result;
}

Result<asset::AssetUri> ResolveUri(const asset::AssetUri& source, const std::string_view relative) {
    const auto slash = source.String().find_last_of('/');
    return asset::AssetUri::Parse(source.String().substr(0, slash + 1) + std::string(relative));
}

Result<std::vector<std::byte>> DecodeDataUri(const std::string_view uri) {
    const auto comma = uri.find(',');
    if (!uri.starts_with("data:") || comma == std::string_view::npos)
        return Err(ErrorCode::ParseInvalidFormat, "invalid data URI");
    const bool base64 = uri.substr(5, comma - 5).ends_with(";base64");
    const auto payload = uri.substr(comma + 1);
    std::vector<std::byte> output;
    if (!base64) {
        for (size_t index = 0; index < payload.size(); ++index) {
            if (payload[index] == '%' && index + 2 < payload.size()) {
                const auto hex = [](const char c) -> i32 { return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1; };
                const i32 high = hex(payload[index + 1]), low = hex(payload[index + 2]);
                if (high < 0 || low < 0)
                    return Err(ErrorCode::ParseInvalidFormat, "invalid data URI escape");
                output.push_back(static_cast<std::byte>((high << 4) | low));
                index += 2;
            } else
                output.push_back(static_cast<std::byte>(static_cast<u8>(payload[index])));
        }
        return Ok(std::move(output));
    }
    u32 accumulator{}, bits{};
    for (const char character : payload) {
        if (character == '=')
            break;
        const std::string_view alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        const auto value = alphabet.find(character);
        if (value == std::string_view::npos)
            return Err(ErrorCode::ParseInvalidFormat, "invalid base64 data URI");
        accumulator = (accumulator << 6U) | static_cast<u32>(value);
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            output.push_back(static_cast<std::byte>((accumulator >> bits) & 0xffU));
        }
    }
    return Ok(std::move(output));
}

Result<void> Hydrate(fastgltf::DataSource& data, const ModelImportRequest& request, ImportedScene& result, ImportedImage* image = nullptr) {
    auto* uri = std::get_if<fastgltf::sources::URI>(&data);
    if (uri == nullptr)
        return Ok();
    if (image != nullptr) {
        image->uri = uri->uri.string();
        image->embedded = uri->uri.isDataUri();
    }
    if (uri->uri.isDataUri()) {
        std::vector<std::byte> bytes;
        TRY_ASSIGN(bytes, DecodeDataUri(uri->uri.string()));
        if (image != nullptr)
            image->payload = bytes;
        data = fastgltf::sources::Vector{std::move(bytes), uri->mimeType};
        return Ok();
    }
    asset::AssetUri resolved = request.source_uri;
    TRY_ASSIGN(resolved, ResolveUri(request.source_uri, uri->uri.string()));
    std::vector<std::byte> bytes;
    TRY_ASSIGN(bytes, request.resolve(resolved));
    result.dependencies.push_back(resolved);
    if (image != nullptr)
        image->payload = bytes;
    data = fastgltf::sources::Vector{std::move(bytes), uri->mimeType};
    return Ok();
}

ImportedTopology Topology(const fastgltf::PrimitiveType type) {
    switch (type) {
        case fastgltf::PrimitiveType::Points:
            return ImportedTopology::Points;
        case fastgltf::PrimitiveType::Lines:
        case fastgltf::PrimitiveType::LineLoop:
        case fastgltf::PrimitiveType::LineStrip:
            return ImportedTopology::Lines;
        case fastgltf::PrimitiveType::Triangles:
            return ImportedTopology::Triangles;
        case fastgltf::PrimitiveType::TriangleStrip:
            return ImportedTopology::TriangleStrip;
        case fastgltf::PrimitiveType::TriangleFan:
            return ImportedTopology::TriangleFan;
    }
    return ImportedTopology::Unsupported;
}

template <typename Source, typename Destination, typename Convert>
void ReadAccessor(const fastgltf::Asset& asset, const size_t accessor, std::vector<Destination>& destination, Convert convert) {
    destination.reserve(asset.accessors[accessor].count);
    fastgltf::iterateAccessor<Source>(asset, asset.accessors[accessor], [&](const Source& value) { destination.push_back(convert(value)); });
}

class FastGltfModelImporter final : public ModelImporter {
public:
    [[nodiscard]] std::string_view Name() const noexcept override {
        return "fastgltf";
    }

    [[nodiscard]] bool Supports(const std::string_view extension, const std::string_view mime) const noexcept override {
        return extension == ".gltf" || extension == ".glb" || extension == "gltf" || extension == "glb" || mime == "model/gltf+json" || mime == "model/gltf-binary";
    }

    [[nodiscard]] Result<ImportedScene> Import(const ModelImportRequest& request) const override {
        auto data = fastgltf::GltfDataBuffer::FromBytes(request.bytes.data(), request.bytes.size());
        if (data.error() != fastgltf::Error::None)
            return Err(ErrorCode::ParseInvalidFormat, "fastgltf could not create a memory input");
        fastgltf::Parser parser(fastgltf::Extensions::KHR_materials_unlit | fastgltf::Extensions::KHR_texture_transform | fastgltf::Extensions::EXT_meshopt_compression);
        auto parsed = parser.loadGltf(data.get(), {}, fastgltf::Options::LoadGLBBuffers | fastgltf::Options::GenerateMeshIndices | fastgltf::Options::DecomposeNodeMatrices);
        if (parsed.error() != fastgltf::Error::None)
            return Err(ErrorCode::ParseInvalidFormat, std::string("fastgltf import failed: ") + std::string(fastgltf::getErrorMessage(parsed.error())));
        fastgltf::Asset& source = parsed.get();
        ImportedScene result;
        result.name = request.source_uri.Path().String();
        result.conversion = {.source_coordinate_system = "glTF right-handed,+Y-up,-Z-forward", .source_units_per_meter = 1.0F, .applied_scale = request.unit_scale};
        result.dependencies.push_back(request.source_uri);
        for (auto& buffer : source.buffers)
            TRY_VOID(Hydrate(buffer.data, request, result));
        result.images.resize(source.images.size());
        for (u32 index = 0; index < source.images.size(); ++index) {
            auto& image = source.images[index];
            auto& record = result.images[index];
            if (const auto* vector = std::get_if<fastgltf::sources::Vector>(&image.data)) {
                record.payload = vector->bytes;
                record.embedded = true;
            }
            TRY_VOID(Hydrate(image.data, request, result, &record));
        }

        result.nodes.resize(source.nodes.size());
        for (u32 index = 0; index < source.nodes.size(); ++index) {
            const auto& node = source.nodes[index];
            auto& output = result.nodes[index];
            output.name = node.name;
            output.local = Matrix(fastgltf::getTransformMatrix(node));
            output.local(0, 3) *= request.unit_scale;
            output.local(1, 3) *= request.unit_scale;
            output.local(2, 3) *= request.unit_scale;
            output.children.assign(node.children.begin(), node.children.end());
            if (node.meshIndex)
                output.meshes.push_back(static_cast<u32>(*node.meshIndex));
            if (node.skinIndex)
                output.skin = static_cast<u32>(*node.skinIndex);
            for (const auto child : node.children) {
                if (child >= result.nodes.size())
                    return Err(ErrorCode::ValidationOutOfRange, "glTF node child is out of range");
                if (result.nodes[child].parent >= 0)
                    return Err(ErrorCode::ValidationInvalidState, "glTF node has multiple parents");
                result.nodes[child].parent = static_cast<i32>(index);
            }
        }
        std::vector<u8> node_state(result.nodes.size());
        const auto world = [&](const auto& self, const u32 node) -> Result<math::mat4f> {
            if (node_state[node] == 1)
                return Err(ErrorCode::ValidationInvalidState, "glTF node hierarchy contains a cycle");
            if (node_state[node] == 2)
                return Ok(result.nodes[node].world);
            node_state[node] = 1;
            const auto parent = result.nodes[node].parent;
            math::mat4f value = result.nodes[node].local;
            if (parent >= 0) {
                math::mat4f parent_world;
                TRY_ASSIGN(parent_world, self(self, static_cast<u32>(parent)));
                value = parent_world * value;
            }
            node_state[node] = 2;
            result.nodes[node].world = value;
            return Ok(value);
        };
        for (u32 index = 0; index < result.nodes.size(); ++index) {
            math::mat4f ignored;
            TRY_ASSIGN(ignored, world(world, index));
        }

        for (const auto& material : source.materials) {
            ImportedMaterial output{.name = std::string(material.name),
                .base_color = Vector(material.pbrData.baseColorFactor),
                .emissive = Vector(material.emissiveFactor),
                .metallic = material.pbrData.metallicFactor,
                .roughness = material.pbrData.roughnessFactor,
                .opacity = material.pbrData.baseColorFactor[3],
                .alpha_cutoff = material.alphaCutoff,
                .normal_scale = material.normalTexture ? material.normalTexture->scale : 1.0F,
                .occlusion_strength = material.occlusionTexture ? material.occlusionTexture->strength : 1.0F,
                .alpha_mode = material.alphaMode == fastgltf::AlphaMode::Mask    ? ImportedAlphaMode::Mask
                              : material.alphaMode == fastgltf::AlphaMode::Blend ? ImportedAlphaMode::Blend
                                                                                 : ImportedAlphaMode::Opaque,
                .double_sided = material.doubleSided};
            const auto texture = [&](const auto& info, const std::string_view semantic) {
                if (!info)
                    return;
                const auto& texture_record = source.textures[info->textureIndex];
                if (!texture_record.imageIndex)
                    return;
                const u32 image = static_cast<u32>(*texture_record.imageIndex);
                output.textures.push_back({std::string(semantic), result.images[image].uri, image});
            };
            texture(material.pbrData.baseColorTexture, "base_color");
            texture(material.pbrData.metallicRoughnessTexture, "metallic_roughness");
            texture(material.normalTexture, "normal");
            texture(material.emissiveTexture, "emissive");
            texture(material.occlusionTexture, "occlusion");
            result.materials.push_back(std::move(output));
        }

        for (u32 mesh_index = 0; mesh_index < source.meshes.size(); ++mesh_index) {
            const auto& mesh = source.meshes[mesh_index];
            ImportedMesh output{.name = std::string(mesh.name)};
            for (const auto& node : result.nodes)
                if (node.skin && std::ranges::find(node.meshes, mesh_index) != node.meshes.end()) {
                    if (output.skin && *output.skin != *node.skin)
                        return Err(ErrorCode::ValidationInvalidState, "glTF mesh is instanced with incompatible skins");
                    output.skin = node.skin;
                }
            for (const auto& primitive : mesh.primitives) {
                ImportedPrimitive converted{.name = output.name, .topology = Topology(primitive.type), .material_slot = primitive.materialIndex ? static_cast<u32>(*primitive.materialIndex) : 0U};
                const auto attribute = [&](const std::string_view name) -> std::optional<size_t> {
                    const auto found = primitive.findAttribute(name);
                    return found == primitive.attributes.end() ? std::nullopt : std::optional<size_t>(found->accessorIndex);
                };
                const auto position = attribute("POSITION");
                if (!position)
                    return Err(ErrorCode::ParseInvalidFormat, "glTF primitive has no POSITION accessor");
                ReadAccessor<fastgltf::math::fvec3>(source, *position, converted.positions, Vector<3>);
                for (auto& value : converted.positions)
                    value *= request.unit_scale;
                if (const auto value = attribute("NORMAL"))
                    ReadAccessor<fastgltf::math::fvec3>(source, *value, converted.normals, Vector<3>);
                if (const auto value = attribute("TANGENT"))
                    ReadAccessor<fastgltf::math::fvec4>(source, *value, converted.tangents, Vector<4>);
                if (const auto value = attribute("TEXCOORD_0"))
                    ReadAccessor<fastgltf::math::fvec2>(source, *value, converted.uv0, Vector<2>);
                if (const auto value = attribute("TEXCOORD_1"))
                    ReadAccessor<fastgltf::math::fvec2>(source, *value, converted.uv1, Vector<2>);
                if (const auto value = attribute("COLOR_0"))
                    ReadAccessor<fastgltf::math::fvec4>(source, *value, converted.colors, Vector<4>);
                if (primitive.indicesAccessor)
                    fastgltf::iterateAccessor<u32>(source, source.accessors[*primitive.indicesAccessor], [&](const u32 value) { converted.indices.push_back(value); });
                std::vector<fastgltf::math::uvec4> joints;
                std::vector<fastgltf::math::fvec4> weights;
                if (const auto value = attribute("JOINTS_0"))
                    ReadAccessor<fastgltf::math::uvec4>(source, *value, joints, [](const auto& item) { return item; });
                if (const auto value = attribute("WEIGHTS_0"))
                    ReadAccessor<fastgltf::math::fvec4>(source, *value, weights, [](const auto& item) { return item; });
                if (!joints.empty() && joints.size() == weights.size()) {
                    converted.influences.resize(joints.size());
                    for (size_t vertex = 0; vertex < joints.size(); ++vertex)
                        for (size_t influence = 0; influence < 4; ++influence)
                            converted.influences[vertex][influence] = {joints[vertex][influence], weights[vertex][influence]};
                }
                if (!converted.positions.empty()) {
                    converted.bounds_min = converted.bounds_max = converted.positions.front();
                    for (const auto& position_value : converted.positions)
                        for (u32 axis = 0; axis < 3; ++axis) {
                            converted.bounds_min[axis] = std::min(converted.bounds_min[axis], position_value[axis]);
                            converted.bounds_max[axis] = std::max(converted.bounds_max[axis], position_value[axis]);
                        }
                }
                output.primitives.push_back(std::move(converted));
            }
            result.meshes.push_back(std::move(output));
        }

        for (const auto& skin : source.skins) {
            ImportedSkeleton skeleton{.name = std::string(skin.name)};
            std::map<size_t, u32> joint_map;
            for (const size_t node : skin.joints) {
                joint_map.emplace(node, static_cast<u32>(skeleton.joints.size()));
                skeleton.joints.push_back({result.nodes[node].name, static_cast<u32>(node), -1, math::mat4f::identity()});
            }
            if (skin.inverseBindMatrices) {
                size_t index{};
                fastgltf::iterateAccessor<fastgltf::math::fmat4x4>(source, source.accessors[*skin.inverseBindMatrices], [&](const auto& value) {
                    if (index < skeleton.joints.size()) {
                        auto& matrix = skeleton.joints[index++].inverse_bind;
                        matrix = Matrix(value);
                        matrix(0, 3) *= request.unit_scale;
                        matrix(1, 3) *= request.unit_scale;
                        matrix(2, 3) *= request.unit_scale;
                    }
                });
            }
            for (auto& joint : skeleton.joints) {
                i32 parent = result.nodes[joint.node].parent;
                while (parent >= 0 && !joint_map.contains(static_cast<size_t>(parent)))
                    parent = result.nodes[static_cast<u32>(parent)].parent;
                if (parent >= 0)
                    joint.parent = static_cast<i32>(joint_map[static_cast<size_t>(parent)]);
            }
            ImportedSkin output_skin{.name = std::string(skin.name), .skeleton = static_cast<u32>(result.skeletons.size())};
            output_skin.joints.resize(skeleton.joints.size());
            std::iota(output_skin.joints.begin(), output_skin.joints.end(), 0U);
            result.skeletons.push_back(std::move(skeleton));
            result.skins.push_back(std::move(output_skin));
        }

        for (u32 animation_index = 0; animation_index < source.animations.size(); ++animation_index) {
            const auto& animation = source.animations[animation_index];
            ImportedAnimationClip clip{.name = animation.name.empty() ? "animation_" + std::to_string(animation_index) : std::string(animation.name), .source_ticks_per_second = 1.0F};
            for (const auto& channel : animation.channels) {
                if (!channel.nodeIndex || channel.path == fastgltf::AnimationPath::Weights)
                    continue;
                const auto& sampler = animation.samplers[channel.samplerIndex];
                if (sampler.interpolation == fastgltf::AnimationInterpolation::CubicSpline)
                    return Err(ErrorCode::GraphicsUnsupportedApi, "glTF cubic spline animation is not supported; tangent serialization and evaluation are required");
                ImportedAnimationChannel output{.node = static_cast<u32>(*channel.nodeIndex),
                    .path = channel.path == fastgltf::AnimationPath::Translation ? AnimationPath::Translation
                            : channel.path == fastgltf::AnimationPath::Rotation  ? AnimationPath::Rotation
                                                                                 : AnimationPath::Scale,
                    .interpolation = sampler.interpolation == fastgltf::AnimationInterpolation::Step          ? AnimationInterpolation::Step
                                     : sampler.interpolation == fastgltf::AnimationInterpolation::CubicSpline ? AnimationInterpolation::CubicSpline
                                                                                                              : AnimationInterpolation::Linear};
                fastgltf::iterateAccessor<f32>(source, source.accessors[sampler.inputAccessor], [&](const f32 value) {
                    output.times.push_back(value);
                    clip.duration = std::max(clip.duration, value);
                });
                if (output.path == AnimationPath::Rotation)
                    ReadAccessor<fastgltf::math::fvec4>(source, sampler.outputAccessor, output.values, Vector<4>);
                else
                    ReadAccessor<fastgltf::math::fvec3>(source, sampler.outputAccessor, output.values, [](const auto& value) { return math::vec4f{value[0], value[1], value[2], 0.0F}; });
                if (output.path == AnimationPath::Translation)
                    for (auto& value : output.values) {
                        value.x *= request.unit_scale;
                        value.y *= request.unit_scale;
                        value.z *= request.unit_scale;
                    }
                clip.channels.push_back(std::move(output));
            }
            result.animations.push_back(std::move(clip));
        }
        TRY_VOID(NormalizeInfluences(result));
        return Ok(std::move(result));
    }
};
} // namespace

ref<const ModelImporter> CreateFastGltfModelImporter() {
    return createRef<const FastGltfModelImporter>();
}

} // namespace woki::gfx
