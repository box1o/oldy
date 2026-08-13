#ifndef WOKI_GFX_MESH_PARSER
#define WOKI_GFX_MESH_PROCESSING
#endif

#include <algorithm>
#include <bit>
#include <cmath>
#include <limits>
#include <map>
#include <numeric>

#ifdef WOKI_GFX_MESH_PROCESSING
#include <meshoptimizer.h>
#include <mikktspace.h>
#endif

#include <woki/gfx/advanced/mesh_product.hpp>
#include "binary_codec.hpp"

namespace woki::gfx {
namespace {
constexpr std::array<std::byte, 4> kMagic{std::byte{'W'}, std::byte{'M'}, std::byte{'S'}, std::byte{'H'}};

#ifdef WOKI_GFX_MESH_PROCESSING
using Writer = detail::BinaryWriter;
#else
class Reader final : public detail::BinaryReader {
public:
    using BinaryReader::BinaryReader;

    bool String(std::string& value) {
        return BinaryReader::String(value, 16U * 1024U * 1024U);
    }
};

bool ChunkValid(const MeshChunk& chunk, const std::span<const std::byte> bytes) {
    return chunk.offset <= bytes.size() && chunk.size <= bytes.size() - chunk.offset && Sha256(bytes.subspan(static_cast<size_t>(chunk.offset), static_cast<size_t>(chunk.size))) == chunk.checksum;
}

bool ReadChunk(Reader& reader, MeshChunk& chunk) {
    return reader.Int(chunk.offset) && reader.Int(chunk.size) && reader.Hash(chunk.checksum);
}
#endif

#ifdef WOKI_GFX_MESH_PROCESSING
void WriteChunk(Writer& writer, const MeshChunk& chunk) {
    writer.Int(chunk.offset);
    writer.Int(chunk.size);
    writer.Hash(chunk.checksum);
}

struct Vertex final {
    math::vec3f position{};
    math::vec3f normal{};
    math::vec4f tangent{};
    math::vec2f uv0{};
    math::vec2f uv1{};
    math::vec4f color{1.0F};
    std::array<u16, 4> joints{};
    std::array<f32, 4> weights{};
};

constexpr u32 kVertexStride = 96;

math::vec3f Cross(const math::vec3f& left, const math::vec3f& right) {
    return {left.y * right.z - left.z * right.y, left.z * right.x - left.x * right.z, left.x * right.y - left.y * right.x};
}

math::vec3f Normalize(const math::vec3f& value) {
    const f32 length = std::sqrt(value.x * value.x + value.y * value.y + value.z * value.z);
    return length > 1.0e-12F ? math::vec3f{value.x / length, value.y / length, value.z / length} : math::vec3f{0.0F, 1.0F, 0.0F};
}

void Triangulate(ImportedPrimitive& primitive) {
    if (primitive.indices.empty()) {
        primitive.indices.resize(primitive.positions.size());
        std::iota(primitive.indices.begin(), primitive.indices.end(), 0U);
    }
    if (primitive.topology == ImportedTopology::TriangleStrip) {
        std::vector<u32> triangles;
        for (size_t index = 2; index < primitive.indices.size(); ++index) {
            if ((index & 1U) == 0)
                triangles.insert(triangles.end(), {primitive.indices[index - 2], primitive.indices[index - 1], primitive.indices[index]});
            else
                triangles.insert(triangles.end(), {primitive.indices[index - 1], primitive.indices[index - 2], primitive.indices[index]});
        }
        primitive.indices = std::move(triangles);
    } else if (primitive.topology == ImportedTopology::TriangleFan) {
        std::vector<u32> triangles;
        for (size_t index = 2; index < primitive.indices.size(); ++index)
            triangles.insert(triangles.end(), {primitive.indices[0], primitive.indices[index - 1], primitive.indices[index]});
        primitive.indices = std::move(triangles);
    }
    primitive.topology = ImportedTopology::Triangles;
}

void GenerateNormals(ImportedPrimitive& primitive) {
    primitive.normals.assign(primitive.positions.size(), {});
    for (size_t index = 0; index + 2 < primitive.indices.size(); index += 3) {
        const u32 a = primitive.indices[index], b = primitive.indices[index + 1], c = primitive.indices[index + 2];
        const auto edge0 = primitive.positions[b] - primitive.positions[a];
        const auto edge1 = primitive.positions[c] - primitive.positions[a];
        const auto normal = Cross(edge0, edge1);
        primitive.normals[a] += normal;
        primitive.normals[b] += normal;
        primitive.normals[c] += normal;
    }
    for (auto& normal : primitive.normals)
        normal = Normalize(normal);
}

struct MikkData {
    ImportedPrimitive* primitive{};
};

u32 MikkVertex(const SMikkTSpaceContext* context, const int face, const int vertex) {
    return static_cast<MikkData*>(context->m_pUserData)->primitive->indices[static_cast<size_t>(face) * 3U + static_cast<size_t>(vertex)];
}

void MikkPosition(const SMikkTSpaceContext* context, float output[], const int face, const int vertex) {
    const auto& value = static_cast<MikkData*>(context->m_pUserData)->primitive->positions[MikkVertex(context, face, vertex)];
    output[0] = value.x;
    output[1] = value.y;
    output[2] = value.z;
}

void MikkNormal(const SMikkTSpaceContext* context, float output[], const int face, const int vertex) {
    const auto& value = static_cast<MikkData*>(context->m_pUserData)->primitive->normals[MikkVertex(context, face, vertex)];
    output[0] = value.x;
    output[1] = value.y;
    output[2] = value.z;
}

void MikkUv(const SMikkTSpaceContext* context, float output[], const int face, const int vertex) {
    const auto& value = static_cast<MikkData*>(context->m_pUserData)->primitive->uv0[MikkVertex(context, face, vertex)];
    output[0] = value.x;
    output[1] = value.y;
}

void MikkSet(const SMikkTSpaceContext* context, const float tangent[], const float sign, const int face, const int vertex) {
    static_cast<MikkData*>(context->m_pUserData)->primitive->tangents[MikkVertex(context, face, vertex)] = {tangent[0], tangent[1], tangent[2], sign};
}

Result<void> GenerateTangents(ImportedPrimitive& primitive) {
    if (primitive.uv0.size() != primitive.positions.size())
        return Err(ErrorCode::ValidationInvalidState, "MikkTSpace tangent generation requires UV0");
    primitive.tangents.assign(primitive.positions.size(), {});
    MikkData data{&primitive};
    SMikkTSpaceInterface interface{};
    interface.m_getNumFaces = [](const SMikkTSpaceContext* context) { return static_cast<int>(static_cast<MikkData*>(context->m_pUserData)->primitive->indices.size() / 3U); };
    interface.m_getNumVerticesOfFace = [](const SMikkTSpaceContext*, int) { return 3; };
    interface.m_getPosition = MikkPosition;
    interface.m_getNormal = MikkNormal;
    interface.m_getTexCoord = MikkUv;
    interface.m_setTSpaceBasic = MikkSet;
    SMikkTSpaceContext context{&interface, &data};
    if (!genTangSpaceDefault(&context))
        return Err(ErrorCode::ValidationInvalidState, "MikkTSpace tangent generation failed");
    return Ok();
}

std::vector<std::byte> Bytes(const std::span<const Vertex> values) {
    Writer writer;
    for (const auto& value : values) {
        for (u32 i = 0; i < 3; ++i)
            writer.Float(value.position[i]);
        for (u32 i = 0; i < 3; ++i)
            writer.Float(value.normal[i]);
        for (u32 i = 0; i < 4; ++i)
            writer.Float(value.tangent[i]);
        for (u32 i = 0; i < 2; ++i)
            writer.Float(value.uv0[i]);
        for (u32 i = 0; i < 2; ++i)
            writer.Float(value.uv1[i]);
        for (u32 i = 0; i < 4; ++i)
            writer.Float(value.color[i]);
        for (const u16 joint : value.joints)
            writer.Int(joint);
        for (const f32 weight : value.weights)
            writer.Float(weight);
    }
    return writer.bytes;
}

std::vector<std::byte> Bytes(const std::span<const u32> values) {
    Writer writer;
    for (const u32 value : values)
        writer.Int(value);
    return writer.bytes;
}

struct BuiltLod {
    f32 ratio{};
    f32 error{};
    std::vector<Vertex> vertices;
    std::vector<u32> indices;
    std::vector<MeshSubmesh> submeshes;
    std::vector<std::byte> meshlets;
};

std::vector<std::byte> BuildMeshlets(const BuiltLod& lod, const MeshletOptions& options) {
    Writer writer;
    if (!options.enabled || lod.indices.empty()) {
        writer.Int<u32>(0);
        return writer.bytes;
    }
    Writer records;
    u32 total{};
    for (u32 submesh_index = 0; submesh_index < lod.submeshes.size(); ++submesh_index) {
        const auto& submesh = lod.submeshes[submesh_index];
        const auto input = std::span(lod.indices).subspan(submesh.first_index, submesh.index_count);
        const size_t bound = meshopt_buildMeshletsBound(input.size(), options.max_vertices, options.max_triangles);
        std::vector<meshopt_Meshlet> meshlets(bound);
        std::vector<u32> vertices(bound * options.max_vertices);
        std::vector<u8> triangles(bound * options.max_triangles * 3U);
        const size_t count = meshopt_buildMeshlets(meshlets.data(), vertices.data(), triangles.data(), input.data(), input.size(), lod.vertices.front().position.data(), lod.vertices.size(), sizeof(Vertex),
            options.max_vertices, options.max_triangles, 0.5F);
        if (count > std::numeric_limits<u32>::max() - total)
            return {};
        total += static_cast<u32>(count);
        for (size_t index = 0; index < count; ++index) {
            const auto& meshlet = meshlets[index];
            const auto bounds = meshopt_computeMeshletBounds(vertices.data() + meshlet.vertex_offset, triangles.data() + meshlet.triangle_offset, meshlet.triangle_count, lod.vertices.front().position.data(),
                lod.vertices.size(), sizeof(Vertex));
            records.Int(submesh_index); // Decode metadata and material boundary.
            records.Int(meshlet.vertex_count);
            records.Int(meshlet.triangle_count);
            for (const float value : bounds.center)
                records.Float(value);
            records.Float(bounds.radius);
            for (const float value : bounds.cone_axis)
                records.Float(value);
            records.Float(bounds.cone_cutoff);
            for (u32 vertex = 0; vertex < meshlet.vertex_count; ++vertex)
                records.Int(vertices[meshlet.vertex_offset + vertex]);
            for (u32 triangle = 0; triangle < meshlet.triangle_count * 3U; ++triangle)
                records.Int(triangles[meshlet.triangle_offset + triangle]);
        }
    }
    writer.Int(total);
    writer.bytes.insert(writer.bytes.end(), records.bytes.begin(), records.bytes.end());
    return writer.bytes;
}

Result<std::vector<BuiltLod>> Cook(const ImportedScene& input, const MeshSource& source) {
    ImportedScene scene = input;
    std::vector<Vertex> vertices;
    std::vector<u32> indices;
    std::vector<MeshSubmesh> submeshes;
    math::vec3f bounds_min(std::numeric_limits<f32>::max()), bounds_max(std::numeric_limits<f32>::lowest());
    for (auto& mesh : scene.meshes)
        for (auto& primitive : mesh.primitives) {
            if (primitive.topology != ImportedTopology::Triangles && primitive.topology != ImportedTopology::TriangleStrip && primitive.topology != ImportedTopology::TriangleFan)
                return Err(ErrorCode::GraphicsUnsupportedApi, "renderable mesh primitive topology is unsupported");
            Triangulate(primitive);
            if (primitive.normals.size() != primitive.positions.size())
                GenerateNormals(primitive);
            if ((source.tangents == TangentPolicy::Generate || (source.tangents == TangentPolicy::GenerateIfMissing && primitive.tangents.size() != primitive.positions.size())) && !primitive.uv0.empty())
                TRY_VOID(GenerateTangents(primitive));
            if (primitive.tangents.size() != primitive.positions.size())
                primitive.tangents.assign(primitive.positions.size(), {1.0F, 0.0F, 0.0F, 1.0F});
            if (primitive.positions.size() > std::numeric_limits<u32>::max() - vertices.size() || primitive.indices.size() > std::numeric_limits<u32>::max() - indices.size())
                return Err(ErrorCode::ValidationOutOfRange, "mesh vertex or index count exceeds 32-bit format limit");
            if (!scene.materials.empty() && primitive.material_slot >= scene.materials.size())
                return Err(ErrorCode::ValidationOutOfRange, "mesh primitive material slot is out of range");
            const u32 base = static_cast<u32>(vertices.size());
            for (size_t index = 0; index < primitive.positions.size(); ++index) {
                Vertex vertex{};
                vertex.position = primitive.positions[index];
                vertex.normal = primitive.normals[index];
                vertex.tangent = primitive.tangents[index];
                if (index < primitive.uv0.size())
                    vertex.uv0 = primitive.uv0[index];
                if (index < primitive.uv1.size())
                    vertex.uv1 = primitive.uv1[index];
                if (index < primitive.colors.size())
                    vertex.color = primitive.colors[index];
                if (index < primitive.influences.size())
                    for (u32 influence = 0; influence < 4; ++influence) {
                        if (primitive.influences[index][influence].joint > std::numeric_limits<u16>::max())
                            return Err(ErrorCode::ValidationOutOfRange, "mesh joint index exceeds 16-bit vertex format limit");
                        vertex.joints[influence] = static_cast<u16>(primitive.influences[index][influence].joint);
                        vertex.weights[influence] = primitive.influences[index][influence].weight;
                    }
                vertices.push_back(vertex);
                for (u32 axis = 0; axis < 3; ++axis) {
                    bounds_min[axis] = std::min(bounds_min[axis], vertex.position[axis]);
                    bounds_max[axis] = std::max(bounds_max[axis], vertex.position[axis]);
                }
            }
            MeshSubmesh submesh{.material_slot = primitive.material_slot,
                .first_index = static_cast<u32>(indices.size()),
                .index_count = static_cast<u32>(primitive.indices.size()),
                .skin = mesh.skin.value_or(std::numeric_limits<u32>::max())};
            for (const u32 index : primitive.indices) {
                if (index >= primitive.positions.size())
                    return Err(ErrorCode::ValidationOutOfRange, "mesh index is out of range");
                indices.push_back(base + index);
            }
            submeshes.push_back(submesh);
        }
    if (vertices.empty() || indices.empty())
        return Err(ErrorCode::ValidationInvalidState, "imported scene contains no renderable indexed triangles");
    if (source.optimize) {
        std::vector<u32> remap(vertices.size());
        const size_t unique = meshopt_generateVertexRemap(remap.data(), indices.data(), indices.size(), vertices.data(), vertices.size(), sizeof(Vertex));
        std::vector<Vertex> remapped_vertices(unique);
        std::vector<u32> remapped_indices(indices.size());
        meshopt_remapVertexBuffer(remapped_vertices.data(), vertices.data(), vertices.size(), sizeof(Vertex), remap.data());
        meshopt_remapIndexBuffer(remapped_indices.data(), indices.data(), indices.size(), remap.data());
        vertices = std::move(remapped_vertices);
        indices = std::move(remapped_indices);
        for (const auto& submesh : submeshes) {
            auto* data = indices.data() + submesh.first_index;
            meshopt_optimizeVertexCache(data, data, submesh.index_count, vertices.size());
            meshopt_optimizeOverdraw(data, data, submesh.index_count, vertices.front().position.data(), vertices.size(), sizeof(Vertex), 1.05F);
        }
        std::vector<u32> fetch(vertices.size());
        meshopt_optimizeVertexFetchRemap(fetch.data(), indices.data(), indices.size(), vertices.size());
        std::vector<Vertex> fetched(vertices.size());
        meshopt_remapVertexBuffer(fetched.data(), vertices.data(), vertices.size(), sizeof(Vertex), fetch.data());
        meshopt_remapIndexBuffer(indices.data(), indices.data(), indices.size(), fetch.data());
        vertices = std::move(fetched);
    }
    std::vector<BuiltLod> lods;
    for (size_t level = 0; level < source.lod_ratios.size(); ++level) {
        BuiltLod lod{.ratio = source.lod_ratios[level], .error = 0.0F, .vertices = vertices, .indices = {}, .submeshes = {}, .meshlets = {}};
        for (const auto& source_submesh : submeshes) {
            const auto input_indices = std::span(indices).subspan(source_submesh.first_index, source_submesh.index_count);
            std::vector<u32> simplified(input_indices.size());
            size_t count = input_indices.size();
            f32 error{};
            if (lod.ratio < 0.9999F) {
                const size_t target = std::max<size_t>(3, static_cast<size_t>(static_cast<f32>(input_indices.size()) * lod.ratio) / 3U * 3U);
                const f32 target_error = source.lod_errors.empty() ? 1.0e-2F : source.lod_errors[level];
                count = meshopt_simplify(simplified.data(), input_indices.data(), input_indices.size(), vertices.front().position.data(), vertices.size(), sizeof(Vertex), target, target_error, meshopt_SimplifyLockBorder,
                    &error);
            } else
                std::ranges::copy(input_indices, simplified.begin());
            simplified.resize(count);
            lod.error = std::max(lod.error, error);
            lod.submeshes.push_back({source_submesh.material_slot, static_cast<u32>(lod.indices.size()), static_cast<u32>(count), 0, source_submesh.skin});
            lod.indices.insert(lod.indices.end(), simplified.begin(), simplified.end());
        }
        lod.meshlets = BuildMeshlets(lod, source.meshlets);
        lods.push_back(std::move(lod));
    }
    std::ranges::reverse(lods);
    return Ok(std::move(lods));
}
#endif
} // namespace

#ifndef WOKI_GFX_MESH_PROCESSING
std::span<const std::byte> MeshProduct::Chunk(const MeshChunk& chunk) const noexcept {
    return chunk.offset <= bytes.size() && chunk.size <= bytes.size() - chunk.offset ? bytes.subspan(static_cast<size_t>(chunk.offset), static_cast<size_t>(chunk.size)) : std::span<const std::byte>{};
}

Result<MeshletStreams> DecodeMeshletStreams(const std::span<const std::byte> bytes) {
    MeshletStreams result;
    size_t cursor{};
    const auto read_u32 = [&]() -> std::optional<u32> {
        if (cursor + sizeof(u32) > bytes.size())
            return std::nullopt;
        u32 value{};
        for (u32 byte = 0; byte < 4; ++byte)
            value |= static_cast<u32>(std::to_integer<u8>(bytes[cursor + byte])) << (byte * 8U);
        cursor += sizeof(u32);
        return value;
    };
    const auto read_f32 = [&]() -> std::optional<f32> {
        const auto bits = read_u32();
        if (!bits)
            return std::nullopt;
        return std::bit_cast<f32>(*bits);
    };
    const auto count = read_u32();
    if (!count || *count > 16'777'216U)
        return Err(ErrorCode::ParseInvalidFormat, "meshlet stream count is invalid");
    result.descriptors.reserve(*count);
    result.bounds.reserve(*count);
    for (u32 index = 0; index < *count; ++index) {
        const auto submesh = read_u32();
        const auto vertex_count = read_u32();
        const auto triangle_count = read_u32();
        if (!submesh || !vertex_count || !triangle_count || *vertex_count > 255 || *triangle_count > 512)
            return Err(ErrorCode::ParseInvalidFormat, "meshlet descriptor is invalid");
        MeshletBounds bounds;
        for (auto& value : bounds.sphere) {
            const auto decoded = read_f32();
            if (!decoded)
                return Err(ErrorCode::ParseInvalidFormat, "meshlet sphere is truncated");
            value = *decoded;
        }
        for (auto& value : bounds.cone) {
            const auto decoded = read_f32();
            if (!decoded)
                return Err(ErrorCode::ParseInvalidFormat, "meshlet cone is truncated");
            value = *decoded;
        }
        MeshletDescriptor descriptor{*submesh, static_cast<u32>(result.vertices.size()), *vertex_count, static_cast<u32>(result.triangles.size()), *triangle_count};
        for (u32 vertex = 0; vertex < *vertex_count; ++vertex) {
            const auto value = read_u32();
            if (!value)
                return Err(ErrorCode::ParseInvalidFormat, "meshlet vertex stream is truncated");
            result.vertices.push_back(*value);
        }
        for (u32 triangle = 0; triangle < *triangle_count * 3U; ++triangle) {
            if (cursor == bytes.size())
                return Err(ErrorCode::ParseInvalidFormat, "meshlet triangle stream is truncated");
            result.triangles.push_back(std::to_integer<u8>(bytes[cursor++]));
        }
        result.descriptors.push_back(descriptor);
        result.bounds.push_back(bounds);
    }
    if (cursor != bytes.size())
        return Err(ErrorCode::ParseInvalidFormat, "meshlet streams contain trailing data");
    return Ok(std::move(result));
}

Result<MeshProduct> ParseMeshProductImpl(const std::span<const std::byte> bytes, const MeshProductLimits limits, const bool validate_payload) {
    if (bytes.size() > limits.max_bytes || bytes.size() < 12)
        return Err(ErrorCode::ParseInvalidFormat, "mesh product size is invalid");
    if (!std::ranges::equal(kMagic, bytes.first(4)))
        return Err(ErrorCode::ParseInvalidFormat, "mesh product magic is invalid");
    Reader reader(bytes.subspan(4));
    u32 version{}, header_size{};
    if (!reader.Int(version) || (version < 2 || version > kMeshProductVersion) || !reader.Int(header_size) || header_size < 12 || header_size > bytes.size())
        return Err(ErrorCode::ParseInvalidFormat, "mesh product header is invalid");
    MeshProduct result;
    result.bytes = validate_payload ? bytes : std::span<const std::byte>{};
    u32 stream_count{};
    if (!reader.Int(stream_count) || stream_count != 1)
        return Err(ErrorCode::ParseInvalidFormat, "mesh stream count is invalid");
    result.schema.streams.resize(stream_count);
    for (auto& stream : result.schema.streams) {
        u8 step{};
        u32 attributes{};
        if (!reader.Int(stream.stride) || !reader.Int(step) || step > 1 || !reader.Int(attributes) || attributes > 32)
            return Err(ErrorCode::ParseInvalidFormat, "mesh vertex stream is invalid");
        stream.step_mode = static_cast<VertexStepMode>(step);
        stream.attributes.resize(attributes);
        for (auto& attribute : stream.attributes) {
            u8 semantic{}, format{};
            if (!reader.Int(semantic) || semantic > static_cast<u8>(VertexSemantic::Weights0) || !reader.Int(attribute.location) || !reader.Int(format) || format > static_cast<u8>(VertexFormat::Unorm16x4)
                || !reader.Int(attribute.offset))
                return Err(ErrorCode::ParseInvalidFormat, "mesh vertex attribute is invalid");
            attribute.semantic = static_cast<VertexSemantic>(semantic);
            attribute.format = static_cast<VertexFormat>(format);
        }
    }
    result.schema.id = HashVertexSchema(result.schema);
    for (u32 axis = 0; axis < 3; ++axis)
        if (!reader.Float(result.bounds_min[axis]))
            return Err(ErrorCode::ParseInvalidFormat, "mesh bounds are invalid");
    for (u32 axis = 0; axis < 3; ++axis)
        if (!reader.Float(result.bounds_max[axis]))
            return Err(ErrorCode::ParseInvalidFormat, "mesh bounds are invalid");
    u32 lod_count{};
    if (!reader.Int(lod_count) || lod_count == 0 || lod_count > limits.max_lods)
        return Err(ErrorCode::ParseInvalidFormat, "mesh LOD count is invalid");
    result.lods.resize(lod_count);
    u32 submesh_total{};
    f32 previous_ratio = -1.0F;
    f32 previous_error = std::numeric_limits<f32>::infinity();
    u32 previous_indices{};
    for (auto& lod : result.lods) {
        u32 submeshes{};
        if (!reader.Float(lod.ratio) || !reader.Float(lod.geometric_error) || !reader.Int(lod.vertex_count) || !reader.Int(lod.index_count) || !ReadChunk(reader, lod.vertices) || !ReadChunk(reader, lod.indices)
            || !ReadChunk(reader, lod.meshlets) || !reader.Int(submeshes) || submeshes > limits.max_submeshes - submesh_total)
            return Err(ErrorCode::ParseInvalidFormat, "mesh LOD record is invalid");
        if (lod.ratio <= previous_ratio || lod.ratio > 1.0F || lod.geometric_error < 0.0F || lod.geometric_error > previous_error || lod.index_count < previous_indices || lod.index_count % 3U != 0)
            return Err(ErrorCode::ParseInvalidFormat, "mesh LOD order is invalid; records must be coarse to fine");
        previous_ratio = lod.ratio;
        previous_error = lod.geometric_error;
        previous_indices = lod.index_count;
        submesh_total += submeshes;
        lod.submeshes.resize(submeshes);
        for (auto& submesh : lod.submeshes)
            if (!reader.Int(submesh.material_slot) || !reader.Int(submesh.first_index) || !reader.Int(submesh.index_count) || !reader.Int(submesh.vertex_offset) || !reader.Int(submesh.skin)
                || submesh.first_index > lod.index_count || submesh.index_count > lod.index_count - submesh.first_index)
                return Err(ErrorCode::ParseInvalidFormat, "mesh submesh is invalid");
    }
    u32 joints{};
    if (!reader.Int(joints) || joints > limits.max_joints)
        return Err(ErrorCode::ParseInvalidFormat, "mesh joint count is invalid");
    result.skeleton.resize(joints);
    std::set<std::string, std::less<>> joint_names;
    for (u32 joint_index = 0; joint_index < result.skeleton.size(); ++joint_index) {
        auto& joint = result.skeleton[joint_index];
        if (!reader.String(joint.name) || joint.name.empty() || !joint_names.insert(joint.name).second || !reader.Int(joint.parent) || joint.parent < -1 || joint.parent >= static_cast<i32>(joint_index)
            || !reader.Matrix(joint.bind_local) || !reader.Matrix(joint.inverse_bind))
            return Err(ErrorCode::ParseInvalidFormat, "mesh joint is invalid");
    }
    u32 clips{};
    if (!reader.Int(clips) || clips > limits.max_clips)
        return Err(ErrorCode::ParseInvalidFormat, "mesh clip count is invalid");
    result.animations.resize(clips);
    u64 key_total{};
    for (auto& clip : result.animations) {
        u32 channels{};
        if (!reader.String(clip.name) || !reader.Int(clip.skeleton) || !reader.Float(clip.duration) || clip.duration < 0.0F || !reader.Int(channels) || channels > limits.max_keys)
            return Err(ErrorCode::ParseInvalidFormat, "mesh clip is invalid");
        clip.channels.resize(channels);
        for (auto& channel : clip.channels) {
            u8 path{}, interpolation{};
            u32 keys{};
            if (!reader.Int(channel.joint) || channel.joint >= joints || !reader.Int(path) || path > 2 || !reader.Int(interpolation) || interpolation > 1 || !reader.Int(keys) || keys == 0
                || key_total + keys > limits.max_keys)
                return Err(ErrorCode::ParseInvalidFormat, "mesh animation channel is invalid");
            channel.path = static_cast<AnimationPath>(path);
            channel.interpolation = static_cast<AnimationInterpolation>(interpolation);
            channel.times.resize(keys);
            channel.values.resize(keys);
            key_total += keys;
            for (auto& time : channel.times)
                if (!reader.Float(time))
                    return Err(ErrorCode::ParseInvalidFormat, "mesh animation time is invalid");
            if (!std::ranges::is_sorted(channel.times) || std::adjacent_find(channel.times.begin(), channel.times.end()) != channel.times.end() || channel.times.back() > clip.duration)
                return Err(ErrorCode::ParseInvalidFormat, "mesh animation times are not finite, unique, sorted, and within duration");
            for (auto& value : channel.values)
                for (u32 component = 0; component < 4; ++component)
                    if (!reader.Float(value[component]))
                        return Err(ErrorCode::ParseInvalidFormat, "mesh animation value is invalid");
        }
    }
    u32 dependencies{};
    if (!reader.Int(dependencies) || dependencies > 4096)
        return Err(ErrorCode::ParseInvalidFormat, "mesh dependency count is invalid");
    result.dependencies.resize(dependencies);
    for (auto& dependency : result.dependencies)
        if (!reader.Id(dependency.asset_id) || !reader.Hash(dependency.product_hash))
            return Err(ErrorCode::ParseInvalidFormat, "mesh dependency is invalid");
    if (!reader.String(result.conversion.source_coordinate_system) || !reader.String(result.conversion.canonical_coordinate_system) || !reader.Float(result.conversion.source_units_per_meter)
        || !reader.Float(result.conversion.applied_scale))
        return Err(ErrorCode::ParseInvalidFormat, "mesh conversion metadata is invalid");
    u8 reflected{};
    if (!reader.Int(reflected) || reflected > 1 || !reader.String(result.importer))
        return Err(ErrorCode::ParseInvalidFormat, "mesh metadata boundary is invalid");
    result.conversion.reflected = reflected != 0;
    u32 node_count{}, skin_count{}, source_skeleton_count{}, material_count{}, image_count{}, diagnostic_count{};
    if (!reader.Int(result.mesh_count) || !reader.Int(node_count) || node_count > 1'000'000)
        return Err(ErrorCode::ParseInvalidFormat, "mesh scene node count is invalid");
    result.nodes.resize(node_count);
    for (u32 node_index = 0; node_index < node_count; ++node_index) {
        auto& node = result.nodes[node_index];
        u32 children{}, meshes{}, skin{};
        if (!reader.String(node.name) || !reader.Int(node.parent) || node.parent < -1 || node.parent >= static_cast<i32>(node_count) || !reader.Int(children) || children > node_count)
            return Err(ErrorCode::ParseInvalidFormat, "mesh scene node is invalid");
        node.children.resize(children);
        for (auto& child : node.children)
            if (!reader.Int(child) || child >= node_count)
                return Err(ErrorCode::ParseInvalidFormat, "mesh child node is invalid");
        if (!reader.Int(meshes) || meshes > result.mesh_count)
            return Err(ErrorCode::ParseInvalidFormat, "mesh node instance count is invalid");
        node.meshes.resize(meshes);
        for (auto& mesh : node.meshes)
            if (!reader.Int(mesh) || mesh >= result.mesh_count)
                return Err(ErrorCode::ParseInvalidFormat, "mesh node instance is invalid");
        if (!reader.Int(skin) || !reader.Matrix(node.local) || !reader.Matrix(node.world))
            return Err(ErrorCode::ParseInvalidFormat, "mesh node transform is invalid");
        if (skin != std::numeric_limits<u32>::max())
            node.skin = skin;
    }
    if (!reader.Int(skin_count) || skin_count > 65'535)
        return Err(ErrorCode::ParseInvalidFormat, "mesh skin count is invalid");
    result.skins.resize(skin_count);
    for (auto& skin : result.skins) {
        u32 count{};
        if (!reader.String(skin.name) || !reader.Int(skin.skeleton) || !reader.Int(count) || count > limits.max_joints)
            return Err(ErrorCode::ParseInvalidFormat, "mesh skin is invalid");
        skin.joints.resize(count);
        for (auto& joint : skin.joints)
            if (!reader.Int(joint))
                return Err(ErrorCode::ParseInvalidFormat, "mesh skin remap is invalid");
    }
    if (!reader.Int(source_skeleton_count) || source_skeleton_count > 65'535)
        return Err(ErrorCode::ParseInvalidFormat, "mesh source skeleton count is invalid");
    result.source_skeletons.resize(source_skeleton_count);
    u64 source_joint_total{};
    for (auto& skeleton : result.source_skeletons) {
        u32 count{};
        if (!reader.String(skeleton.name) || !reader.Int(count) || source_joint_total + count > limits.max_joints)
            return Err(ErrorCode::ParseInvalidFormat, "mesh source skeleton is invalid");
        source_joint_total += count;
        skeleton.joints.resize(count);
        std::set<std::string, std::less<>> source_names;
        for (u32 joint_index = 0; joint_index < count; ++joint_index) {
            auto& joint = skeleton.joints[joint_index];
            if (!reader.String(joint.name) || joint.name.empty() || !source_names.insert(joint.name).second || !reader.Int(joint.node) || joint.node >= node_count || !reader.Int(joint.parent) || joint.parent < -1
                || joint.parent >= static_cast<i32>(joint_index) || !reader.Matrix(joint.inverse_bind))
                return Err(ErrorCode::ParseInvalidFormat, "mesh source joint is invalid");
        }
    }
    if (!reader.Int(material_count) || material_count > limits.max_submeshes)
        return Err(ErrorCode::ParseInvalidFormat, "mesh material count is invalid");
    result.materials.resize(material_count);
    for (auto& material : result.materials) {
        u32 textures{};
        if (!reader.String(material.name))
            return Err(ErrorCode::ParseInvalidFormat, "mesh material name is invalid");
        for (u32 i = 0; i < 4; ++i)
            if (!reader.Float(material.base_color[i]))
                return Err(ErrorCode::ParseInvalidFormat, "mesh material color is invalid");
        for (u32 i = 0; i < 3; ++i)
            if (!reader.Float(material.emissive[i]))
                return Err(ErrorCode::ParseInvalidFormat, "mesh material emissive is invalid");
        if (!reader.Float(material.metallic) || !reader.Float(material.roughness) || !reader.Float(material.opacity))
            return Err(ErrorCode::ParseInvalidFormat, "mesh material is invalid");
        if (version >= 3) {
            u8 alpha_mode{}, double_sided{};
            if (!reader.Float(material.alpha_cutoff) || !reader.Float(material.normal_scale) || !reader.Float(material.occlusion_strength) || !reader.Int(alpha_mode)
                || alpha_mode > static_cast<u8>(ImportedAlphaMode::Blend) || !reader.Int(double_sided) || double_sided > 1)
                return Err(ErrorCode::ParseInvalidFormat, "mesh material render metadata is invalid");
            material.alpha_mode = static_cast<ImportedAlphaMode>(alpha_mode);
            material.double_sided = double_sided != 0;
        }
        if (!reader.Int(textures) || textures > 256)
            return Err(ErrorCode::ParseInvalidFormat, "mesh material texture count is invalid");
        material.textures.resize(textures);
        for (auto& texture : material.textures)
            if (!reader.String(texture.semantic) || !reader.String(texture.uri) || !reader.Int(texture.image))
                return Err(ErrorCode::ParseInvalidFormat, "mesh material texture is invalid");
    }
    if (version >= 4) {
        u32 generated_material_count{};
        if (!reader.Int(generated_material_count) || generated_material_count != material_count)
            return Err(ErrorCode::ParseInvalidFormat, "mesh generated material slot count is invalid");
        result.generated_material_instances.resize(generated_material_count);
        for (auto& id : result.generated_material_instances)
            if (!reader.Id(id) || !id)
                return Err(ErrorCode::ParseInvalidFormat, "mesh generated material identity is invalid");
    }
    if (!reader.Int(image_count) || image_count > 65'535)
        return Err(ErrorCode::ParseInvalidFormat, "mesh image count is invalid");
    result.images.resize(image_count);
    for (auto& image : result.images) {
        u8 embedded{};
        if (!reader.String(image.uri) || !reader.String(image.mime_type) || !reader.Int(embedded) || embedded > 1 || !ReadChunk(reader, image.payload))
            return Err(ErrorCode::ParseInvalidFormat, "mesh image is invalid");
        image.embedded = embedded != 0;
    }
    for (const auto& node : result.nodes)
        if (node.skin && *node.skin >= result.skins.size())
            return Err(ErrorCode::ParseInvalidFormat, "mesh node skin is invalid");
    for (u32 parent = 0; parent < result.nodes.size(); ++parent) {
        std::set<u32> children;
        for (const u32 child : result.nodes[parent].children)
            if (!children.insert(child).second || result.nodes[child].parent != static_cast<i32>(parent))
                return Err(ErrorCode::ParseInvalidFormat, "mesh node child/parent relationship is inconsistent");
    }
    for (u32 child = 0; child < result.nodes.size(); ++child)
        if (result.nodes[child].parent >= 0) {
            const auto& siblings = result.nodes[static_cast<u32>(result.nodes[child].parent)].children;
            if (std::ranges::find(siblings, child) == siblings.end())
                return Err(ErrorCode::ParseInvalidFormat, "mesh node is missing from its parent's children");
        }
    for (const auto& skin : result.skins) {
        if (skin.skeleton >= result.source_skeletons.size())
            return Err(ErrorCode::ParseInvalidFormat, "mesh skin skeleton is invalid");
        for (const u32 joint : skin.joints)
            if (joint >= result.source_skeletons[skin.skeleton].joints.size())
                return Err(ErrorCode::ParseInvalidFormat, "mesh skin joint remap is invalid");
    }
    std::vector<u8> node_state(result.nodes.size());
    const auto visit_node = [&](const auto& self, const u32 index) -> bool {
        if (node_state[index] == 1)
            return false;
        if (node_state[index] == 2)
            return true;
        node_state[index] = 1;
        const i32 parent = result.nodes[index].parent;
        if (parent >= 0 && !self(self, static_cast<u32>(parent)))
            return false;
        node_state[index] = 2;
        return true;
    };
    for (u32 index = 0; index < result.nodes.size(); ++index)
        if (!visit_node(visit_node, index))
            return Err(ErrorCode::ParseInvalidFormat, "mesh node hierarchy contains a cycle");
    for (const auto& material : result.materials)
        for (const auto& texture : material.textures)
            if (texture.image >= result.images.size())
                return Err(ErrorCode::ParseInvalidFormat, "mesh material image is invalid");
    for (const auto& lod : result.lods)
        for (const auto& submesh : lod.submeshes)
            if ((!result.materials.empty() && submesh.material_slot >= result.materials.size()) || (submesh.skin != std::numeric_limits<u32>::max() && submesh.skin >= result.skins.size()))
                return Err(ErrorCode::ParseInvalidFormat, "mesh submesh material slot is invalid");
    for (const auto& clip : result.animations)
        if (!result.source_skeletons.empty() && clip.skeleton >= result.source_skeletons.size())
            return Err(ErrorCode::ParseInvalidFormat, "mesh animation skeleton is invalid");
    if (!reader.Int(diagnostic_count) || diagnostic_count > 65'535)
        return Err(ErrorCode::ParseInvalidFormat, "mesh diagnostic count is invalid");
    result.diagnostics.resize(diagnostic_count);
    for (auto& diagnostic : result.diagnostics) {
        u8 severity{};
        if (!reader.Int(severity) || severity > static_cast<u8>(DiagnosticSeverity::Error) || !reader.String(diagnostic.code) || !reader.String(diagnostic.message))
            return Err(ErrorCode::ParseInvalidFormat, "mesh diagnostic is invalid");
        diagnostic.severity = static_cast<DiagnosticSeverity>(severity);
    }
    if (reader.Offset() + 4 != header_size)
        return Err(ErrorCode::ParseInvalidFormat, "mesh metadata has trailing data");
    std::vector<std::pair<u64, u64>> ranges;
    for (auto& lod : result.lods) {
        lod.vertices.offset += header_size;
        lod.indices.offset += header_size;
        lod.meshlets.offset += header_size;
        if (result.schema.streams.front().stride == 0 || lod.vertex_count > std::numeric_limits<u64>::max() / result.schema.streams.front().stride
            || lod.vertices.size != static_cast<u64>(lod.vertex_count) * result.schema.streams.front().stride || lod.indices.size != static_cast<u64>(lod.index_count) * sizeof(u32))
            return Err(ErrorCode::ParseInvalidFormat, "mesh vertex or index byte size does not match its descriptor");
        for (const auto* chunk : {&lod.vertices, &lod.indices, &lod.meshlets}) {
            if (chunk->offset > std::numeric_limits<u64>::max() - chunk->size)
                return Err(ErrorCode::ParseInvalidFormat, "mesh chunk range overflows");
            ranges.emplace_back(chunk->offset, chunk->offset + chunk->size);
        }
        if (validate_payload && (!ChunkValid(lod.vertices, bytes) || !ChunkValid(lod.indices, bytes) || !ChunkValid(lod.meshlets, bytes)))
            return Err(ErrorCode::ParseInvalidFormat, "mesh chunk range or checksum is invalid");
    }
    for (auto& image : result.images) {
        if (image.payload.offset > std::numeric_limits<u64>::max() - header_size || image.payload.size > std::numeric_limits<u64>::max() - (image.payload.offset + header_size))
            return Err(ErrorCode::ParseInvalidFormat, "mesh image payload range overflows");
        image.payload.offset += header_size;
        ranges.emplace_back(image.payload.offset, image.payload.offset + image.payload.size);
        if (validate_payload && !ChunkValid(image.payload, bytes))
            return Err(ErrorCode::ParseInvalidFormat, "mesh image payload is invalid");
    }
    std::ranges::sort(ranges);
    for (size_t index = 1; index < ranges.size(); ++index)
        if (ranges[index].first < ranges[index - 1].second)
            return Err(ErrorCode::ParseInvalidFormat, "mesh chunks overlap");
    if (validate_payload && (!ranges.empty() && ranges.back().second != bytes.size()))
        return Err(ErrorCode::ParseInvalidFormat, "mesh product has missing or trailing payload data");
    return Ok(std::move(result));
}

Result<MeshProduct> ParseMeshProduct(const std::span<const std::byte> bytes, const MeshProductLimits limits) {
    return ParseMeshProductImpl(bytes, limits, true);
}

Result<MeshProduct> ParseMeshProductHeader(const std::span<const std::byte> bytes, const MeshProductLimits limits) {
    if (bytes.size() < 12)
        return Err(ErrorCode::ParseInvalidFormat, "mesh product header prefix is truncated");
    u32 header_size{};
    for (u32 index = 0; index < 4; ++index)
        header_size |= static_cast<u32>(std::to_integer<u8>(bytes[8 + index])) << (index * 8U);
    if (header_size < 12 || header_size > bytes.size() || header_size > limits.max_bytes)
        return Err(ErrorCode::ParseInvalidFormat, "mesh product header range is invalid");
    return ParseMeshProductImpl(bytes.first(header_size), limits, false);
}

Result<MeshProduct> ReadMeshProductHeader(const asset::ProductReader& reader, const MeshProductLimits limits) {
    if (reader.Header().type != kMeshProductType || reader.Chunks().empty() || reader.Chunks().front().semantic != asset::ProductChunkSemantic::Header)
        return Err(ErrorCode::ParseInvalidFormat, "asset product does not expose a mesh header chunk");
    auto bytes = reader.ReadChunk(0);
    return bytes ? ParseMeshProductHeader(*bytes, limits) : Result<MeshProduct>(Err(std::move(bytes).error()));
}

Result<MeshLodPayload> ReadMeshLod(const asset::ProductReader& reader, const u32 lod) {
    u32 current{};
    MeshLodPayload result;
    bool vertices{}, indices{}, meshlets{};
    for (std::size_t index = 1; index < reader.Chunks().size(); ++index) {
        const auto semantic = reader.Chunks()[index].semantic;
        if (semantic == asset::ProductChunkSemantic::MeshVertices) {
            if (current == lod) {
                TRY_ASSIGN(result.vertices, reader.ReadChunk(index));
                vertices = true;
            }
        } else if (semantic == asset::ProductChunkSemantic::MeshIndices) {
            if (current == lod) {
                TRY_ASSIGN(result.indices, reader.ReadChunk(index));
                indices = true;
            }
        } else if (semantic == asset::ProductChunkSemantic::Meshlets) {
            if (current == lod) {
                TRY_ASSIGN(result.meshlets, reader.ReadChunk(index));
                meshlets = true;
                return vertices && indices ? Ok(std::move(result)) : Result<MeshLodPayload>(Err(ErrorCode::ParseInvalidFormat, "mesh LOD chunks are incomplete"));
            }
            ++current;
        }
    }
    return Err(ErrorCode::OutOfRange, meshlets ? "mesh LOD chunks are incomplete" : "mesh LOD is unavailable");
}
#endif

#ifdef WOKI_GFX_MESH_PROCESSING
Result<std::vector<std::byte>> BuildMeshProduct(const ImportedScene& scene,
    const MeshSource& source,
    const std::string_view importer,
    const std::span<const asset::ProductDependency> dependencies,
    const MeshProductLimits limits) {
    if (source.lod_ratios.empty() || source.lod_ratios.size() > limits.max_lods)
        return Err(ErrorCode::ValidationOutOfRange, "mesh LOD count exceeds configured limit");
    std::vector<BuiltLod> built;
    TRY_ASSIGN(built, Cook(scene, source));
    VertexSchema schema{.streams = {{kVertexStride, VertexStepMode::Vertex,
                            {{VertexSemantic::Position, 0, VertexFormat::Float32x3, 0}, {VertexSemantic::Normal, 1, VertexFormat::Float32x3, 12}, {VertexSemantic::Tangent, 2, VertexFormat::Float32x4, 24},
                                {VertexSemantic::TexCoord0, 3, VertexFormat::Float32x2, 40}, {VertexSemantic::TexCoord1, 4, VertexFormat::Float32x2, 48}, {VertexSemantic::Color0, 5, VertexFormat::Float32x4, 56},
                                {VertexSemantic::Joints0, 6, VertexFormat::Uint16x4, 72}, {VertexSemantic::Weights0, 7, VertexFormat::Float32x4, 80}}}}};
    schema.id = HashVertexSchema(schema);
    std::vector<std::byte> data;
    std::vector<MeshLod> lods;
    const auto append = [&](const std::span<const std::byte> chunk) {
        MeshChunk result{static_cast<u64>(data.size()), static_cast<u64>(chunk.size()), Sha256(chunk)};
        data.insert(data.end(), chunk.begin(), chunk.end());
        return result;
    };
    for (const auto& source_lod : built) {
        if (source_lod.vertices.size() > std::numeric_limits<u32>::max() || source_lod.indices.size() > std::numeric_limits<u32>::max() || source_lod.submeshes.size() > limits.max_submeshes)
            return Err(ErrorCode::ValidationOutOfRange, "cooked mesh count exceeds product limits");
        const auto vertex_bytes = Bytes(source_lod.vertices);
        const auto index_bytes = Bytes(source_lod.indices);
        lods.push_back({source_lod.ratio, source_lod.error, static_cast<u32>(source_lod.vertices.size()), static_cast<u32>(source_lod.indices.size()), append(vertex_bytes), append(index_bytes),
            append(source_lod.meshlets), source_lod.submeshes});
    }
    std::vector<MeshImage> images;
    images.reserve(scene.images.size());
    for (const auto& image : scene.images)
        images.push_back({image.uri, image.mime_type, image.embedded, append(image.payload)});
    math::vec3f bounds_min(std::numeric_limits<f32>::max()), bounds_max(std::numeric_limits<f32>::lowest());
    for (const auto& vertex : built.front().vertices)
        for (u32 axis = 0; axis < 3; ++axis) {
            bounds_min[axis] = std::min(bounds_min[axis], vertex.position[axis]);
            bounds_max[axis] = std::max(bounds_max[axis], vertex.position[axis]);
        }
    std::map<u32, u32> node_to_joint;
    std::vector<MeshJoint> joints;
    if (!scene.skeletons.empty()) {
        const auto& source_joints = scene.skeletons.front().joints;
        if (source_joints.size() > limits.max_joints)
            return Err(ErrorCode::ValidationOutOfRange, "mesh skeleton exceeds configured joint limit");
        std::set<std::string, std::less<>> names;
        std::vector<u8> state(source_joints.size());
        std::vector<u32> order;
        const auto visit = [&](const auto& self, const u32 index) -> Result<void> {
            if (index >= source_joints.size())
                return Err(ErrorCode::ValidationOutOfRange, "skeleton parent is out of range");
            if (state[index] == 1)
                return Err(ErrorCode::ValidationInvalidState, "skeleton contains a parent cycle");
            if (state[index] == 2)
                return Ok();
            state[index] = 1;
            const i32 parent = source_joints[index].parent;
            if (parent >= 0)
                TRY_VOID(self(self, static_cast<u32>(parent)));
            state[index] = 2;
            order.push_back(index);
            return Ok();
        };
        for (u32 index = 0; index < source_joints.size(); ++index)
            TRY_VOID(visit(visit, index));
        std::vector<u32> remap(source_joints.size());
        for (u32 index = 0; index < order.size(); ++index)
            remap[order[index]] = index;
        for (const u32 old_index : order) {
            const auto& joint = source_joints[old_index];
            if (joint.node >= scene.nodes.size())
                return Err(ErrorCode::ValidationOutOfRange, "skeleton joint node is out of range");
            if (joint.name.empty() || !names.insert(joint.name).second)
                return Err(ErrorCode::ValidationInvalidState, "skeleton contains an empty or duplicate joint name");
            const i32 parent = joint.parent < 0 ? -1 : static_cast<i32>(remap[static_cast<u32>(joint.parent)]);
            const math::mat4f bind_local = parent < 0 ? scene.nodes[joint.node].world : scene.nodes[source_joints[static_cast<u32>(joint.parent)].node].world.inverse() * scene.nodes[joint.node].world;
            const u32 new_index = static_cast<u32>(joints.size());
            node_to_joint.emplace(joint.node, new_index);
            joints.push_back({joint.name, parent, bind_local, joint.inverse_bind});
        }
    }
    std::vector<MeshAnimationClip> clips;
    if (source.import_animations)
        for (const auto& clip : scene.animations) {
            if (!source.animations.empty() && std::ranges::find(source.animations, clip.name) == source.animations.end())
                continue;
            MeshAnimationClip output{clip.name, 0, clip.duration, {}};
            for (const auto& channel : clip.channels) {
                const auto joint = node_to_joint.find(channel.node);
                if (joint != node_to_joint.end()) {
                    if (channel.interpolation == AnimationInterpolation::CubicSpline)
                        return Err(ErrorCode::GraphicsUnsupportedApi, "cubic spline animation requires tangent serialization and evaluation");
                    if (channel.times.empty() || channel.times.size() != channel.values.size() || !std::ranges::is_sorted(channel.times)
                        || std::adjacent_find(channel.times.begin(), channel.times.end()) != channel.times.end())
                        return Err(ErrorCode::ValidationInvalidState, "animation keys must have matching values and finite, unique, sorted times");
                    for (const f32 time : channel.times)
                        if (!std::isfinite(time) || time < 0.0F || time > clip.duration)
                            return Err(ErrorCode::ValidationOutOfRange, "animation key time is outside clip duration");
                    for (const auto& value : channel.values)
                        for (u32 component = 0; component < 4; ++component)
                            if (!std::isfinite(value[component]))
                                return Err(ErrorCode::ValidationInvalidState, "animation value is not finite");
                    output.channels.push_back({joint->second, channel.path, channel.interpolation, channel.times, channel.values});
                }
            }
            if (!output.channels.empty())
                clips.push_back(std::move(output));
        }
    Writer header;
    for (const auto byte : kMagic)
        header.bytes.push_back(byte);
    header.Int(kMeshProductVersion);
    header.Int<u32>(0);
    header.Int(static_cast<u32>(schema.streams.size()));
    for (const auto& stream : schema.streams) {
        header.Int(stream.stride);
        header.Int(static_cast<u8>(stream.step_mode));
        header.Int(static_cast<u32>(stream.attributes.size()));
        for (const auto& attribute : stream.attributes) {
            header.Int(static_cast<u8>(attribute.semantic));
            header.Int(attribute.location);
            header.Int(static_cast<u8>(attribute.format));
            header.Int(attribute.offset);
        }
    }
    for (u32 axis = 0; axis < 3; ++axis)
        header.Float(bounds_min[axis]);
    for (u32 axis = 0; axis < 3; ++axis)
        header.Float(bounds_max[axis]);
    header.Int(static_cast<u32>(lods.size()));
    for (const auto& lod : lods) {
        header.Float(lod.ratio);
        header.Float(lod.geometric_error);
        header.Int(lod.vertex_count);
        header.Int(lod.index_count);
        WriteChunk(header, lod.vertices);
        WriteChunk(header, lod.indices);
        WriteChunk(header, lod.meshlets);
        header.Int(static_cast<u32>(lod.submeshes.size()));
        for (const auto& submesh : lod.submeshes) {
            header.Int(submesh.material_slot);
            header.Int(submesh.first_index);
            header.Int(submesh.index_count);
            header.Int(submesh.vertex_offset);
            header.Int(submesh.skin);
        }
    }
    header.Int(static_cast<u32>(joints.size()));
    for (const auto& joint : joints) {
        header.String(joint.name);
        header.Int(joint.parent);
        header.Matrix(joint.bind_local);
        header.Matrix(joint.inverse_bind);
    }
    header.Int(static_cast<u32>(clips.size()));
    for (const auto& clip : clips) {
        header.String(clip.name);
        header.Int(clip.skeleton);
        header.Float(clip.duration);
        header.Int(static_cast<u32>(clip.channels.size()));
        for (const auto& channel : clip.channels) {
            header.Int(channel.joint);
            header.Int(static_cast<u8>(channel.path));
            header.Int(static_cast<u8>(channel.interpolation));
            header.Int(static_cast<u32>(channel.times.size()));
            for (const f32 time : channel.times)
                header.Float(time);
            for (const auto& value : channel.values)
                for (u32 component = 0; component < 4; ++component)
                    header.Float(value[component]);
        }
    }
    header.Int(static_cast<u32>(dependencies.size()));
    for (const auto& dependency : dependencies) {
        header.Id(dependency.asset_id);
        header.Hash(dependency.product_hash);
    }
    header.String(scene.conversion.source_coordinate_system);
    header.String(scene.conversion.canonical_coordinate_system);
    header.Float(scene.conversion.source_units_per_meter);
    header.Float(scene.conversion.applied_scale);
    header.Int(static_cast<u8>(scene.conversion.reflected));
    header.String(importer);
    if (scene.meshes.size() > std::numeric_limits<u32>::max() || scene.nodes.size() > std::numeric_limits<u32>::max() || scene.skins.size() > std::numeric_limits<u32>::max()
        || scene.materials.size() > std::numeric_limits<u32>::max() || images.size() > std::numeric_limits<u32>::max() || scene.diagnostics.size() > std::numeric_limits<u32>::max())
        return Err(ErrorCode::ValidationOutOfRange, "mesh scene metadata exceeds 32-bit format limits");
    header.Int(static_cast<u32>(scene.meshes.size()));
    header.Int(static_cast<u32>(scene.nodes.size()));
    for (const auto& node : scene.nodes) {
        header.String(node.name);
        header.Int(node.parent);
        header.Int(static_cast<u32>(node.children.size()));
        for (const u32 child : node.children)
            header.Int(child);
        header.Int(static_cast<u32>(node.meshes.size()));
        for (const u32 mesh : node.meshes)
            header.Int(mesh);
        header.Int(node.skin.value_or(std::numeric_limits<u32>::max()));
        header.Matrix(node.local);
        header.Matrix(node.world);
    }
    header.Int(static_cast<u32>(scene.skins.size()));
    for (const auto& skin : scene.skins) {
        header.String(skin.name);
        header.Int(skin.skeleton);
        header.Int(static_cast<u32>(skin.joints.size()));
        for (const u32 joint : skin.joints)
            header.Int(joint);
    }
    header.Int(static_cast<u32>(scene.skeletons.size()));
    for (const auto& skeleton : scene.skeletons) {
        header.String(skeleton.name);
        header.Int(static_cast<u32>(skeleton.joints.size()));
        for (const auto& joint : skeleton.joints) {
            header.String(joint.name);
            header.Int(joint.node);
            header.Int(joint.parent);
            header.Matrix(joint.inverse_bind);
        }
    }
    header.Int(static_cast<u32>(scene.materials.size()));
    for (const auto& material : scene.materials) {
        header.String(material.name);
        for (u32 i = 0; i < 4; ++i)
            header.Float(material.base_color[i]);
        for (u32 i = 0; i < 3; ++i)
            header.Float(material.emissive[i]);
        header.Float(material.metallic);
        header.Float(material.roughness);
        header.Float(material.opacity);
        header.Float(material.alpha_cutoff);
        header.Float(material.normal_scale);
        header.Float(material.occlusion_strength);
        header.Int(static_cast<u8>(material.alpha_mode));
        header.Int(static_cast<u8>(material.double_sided));
        header.Int(static_cast<u32>(material.textures.size()));
        for (const auto& texture : material.textures) {
            header.String(texture.semantic);
            header.String(texture.uri);
            header.Int(texture.image);
        }
    }
    header.Int(static_cast<u32>(scene.materials.size()));
    for (u32 slot = 0; slot < scene.materials.size(); ++slot)
        header.Id(asset::AssetId::FromName(source.asset_id.String() + "/material/" + std::to_string(slot) + "/instance"));
    header.Int(static_cast<u32>(images.size()));
    for (const auto& image : images) {
        header.String(image.uri);
        header.String(image.mime_type);
        header.Int(static_cast<u8>(image.embedded));
        WriteChunk(header, image.payload);
    }
    header.Int(static_cast<u32>(scene.diagnostics.size()));
    for (const auto& diagnostic : scene.diagnostics) {
        header.Int(static_cast<u8>(diagnostic.severity));
        header.String(diagnostic.code);
        header.String(diagnostic.message);
    }
    if (header.bytes.size() > std::numeric_limits<u32>::max())
        return Err(ErrorCode::ValidationOutOfRange, "mesh product header exceeds 32-bit format limit");
    const u32 header_size = static_cast<u32>(header.bytes.size());
    for (u32 index = 0; index < 4; ++index)
        header.bytes[8 + index] = static_cast<std::byte>((header_size >> (index * 8U)) & 0xffU);
    if (header.bytes.size() + data.size() > limits.max_bytes)
        return Err(ErrorCode::ValidationOutOfRange, "mesh product exceeds configured byte limit");
    header.bytes.insert(header.bytes.end(), data.begin(), data.end());
    return Ok(std::move(header.bytes));
}
#endif
} // namespace woki::gfx
