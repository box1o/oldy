#include <array>
#include <limits>
#include <functional>

#include <woki/gfx/advanced/product.hpp>
#include "binary_codec.hpp"

namespace woki::gfx {
namespace {

constexpr std::array<std::byte, 4> kMagic{std::byte{'W'}, std::byte{'S'}, std::byte{'H'}, std::byte{'D'}};

using Writer = detail::BinaryWriter;

class Reader : public detail::BinaryReader {
public:
    Reader(const std::span<const std::byte> bytes, const ShaderPayloadLimits limits)
        : BinaryReader(bytes),
          limits_(limits) {}

    bool String(std::string& value) {
        return BinaryReader::String(value, limits_.max_string_bytes);
    }

private:
    ShaderPayloadLimits limits_;
};

template <typename T, typename F>
bool Vector(Reader& reader, std::vector<T>& values, const u32 maximum, F read) {
    u32 count = 0;
    if (!reader.Int(count) || count > maximum)
        return false;
    values.resize(count);
    for (T& value : values)
        if (!read(value))
            return false;
    return true;
}

void WriteInterface(Writer& writer, const ShaderInterface& interface) {
    std::function<void(const BufferMemberLayout&)> write_member;
    write_member = [&](const BufferMemberLayout& member) {
        writer.String(member.name);
        writer.String(member.type);
        writer.Int(member.offset);
        writer.Int(member.size);
        writer.Int(member.alignment);
        writer.Int(member.array_stride);
        writer.Int(member.matrix_stride);
        writer.Int(static_cast<u32>(member.members.size()));
        for (const auto& nested : member.members)
            write_member(nested);
    };
    writer.Int(static_cast<u32>(interface.entry_points.size()));
    for (const auto& entry : interface.entry_points) {
        writer.String(entry.name);
        writer.String(entry.semantic);
        writer.Int(static_cast<u8>(entry.stage));
        writer.Int(static_cast<u32>(entry.inputs.size()));
        for (const auto& io : entry.inputs) {
            writer.Int(io.location);
            writer.Int(static_cast<u8>(io.type));
        }
        writer.Int(static_cast<u32>(entry.outputs.size()));
        for (const auto& io : entry.outputs) {
            writer.Int(io.location);
            writer.Int(static_cast<u8>(io.type));
        }
        for (const u32 size : entry.workgroup_size)
            writer.Int(size);
    }
    writer.Int(static_cast<u32>(interface.bindings.size()));
    for (const auto& binding : interface.bindings) {
        writer.Int(binding.group);
        writer.Int(binding.binding);
        writer.Int(binding.stages);
        writer.Int(static_cast<u8>(binding.kind));
        writer.Int(static_cast<u8>(binding.dimension));
        writer.Int(static_cast<u8>(binding.sample_type));
        writer.Int(static_cast<u8>(binding.storage_access));
        writer.Int(static_cast<u8>(binding.storage_format));
        writer.Int(binding.min_binding_size);
        writer.Int(binding.array_size);
        writer.String(binding.semantic);
        writer.String(binding.group_semantic);
        writer.Int(static_cast<u32>(binding.visible_entries.size()));
        for (const auto& entry : binding.visible_entries)
            writer.String(entry);
        writer.String(binding.buffer_type);
        writer.Int(static_cast<u32>(binding.buffer_members.size()));
        for (const auto& member : binding.buffer_members)
            write_member(member);
    }
    writer.Int(static_cast<u32>(interface.overrides.size()));
    for (const auto& value : interface.overrides) {
        writer.String(value.name);
        writer.Int(value.id);
        writer.Int(static_cast<u8>(value.type));
        writer.Int(static_cast<u8>(value.has_default));
    }
    writer.Int(static_cast<u32>(interface.capabilities.size()));
    for (const auto& capability : interface.capabilities)
        writer.String(capability);
}

bool ReadInterface(Reader& reader, ShaderInterface& interface, const u32 maximum) {
    std::function<bool(BufferMemberLayout&)> read_member;
    read_member = [&](BufferMemberLayout& member) {
        return reader.String(member.name) && reader.String(member.type) && reader.Int(member.offset) && reader.Int(member.size) && reader.Int(member.alignment) && reader.Int(member.array_stride)
               && reader.Int(member.matrix_stride) && Vector(reader, member.members, maximum, [&](BufferMemberLayout& nested) { return read_member(nested); });
    };
    if (!Vector(reader, interface.entry_points, maximum, [&](EntryPointInfo& entry) {
            u8 stage = 0;
            if (!reader.String(entry.name) || !reader.String(entry.semantic) || !reader.Int(stage) || stage > static_cast<u8>(ShaderStage::Compute))
                return false;
            entry.stage = static_cast<ShaderStage>(stage);
            auto io = [&](StageIo& value) {
                u8 type = 0;
                if (!reader.Int(value.location) || !reader.Int(type) || type > static_cast<u8>(ValueType::Vec4U))
                    return false;
                value.type = static_cast<ValueType>(type);
                return true;
            };
            if (!Vector(reader, entry.inputs, maximum, io) || !Vector(reader, entry.outputs, maximum, io))
                return false;
            return reader.Int(entry.workgroup_size[0]) && reader.Int(entry.workgroup_size[1]) && reader.Int(entry.workgroup_size[2]);
        }))
        return false;
    if (!Vector(reader, interface.bindings, maximum, [&](BindingInfo& binding) {
            u8 kind = 0, dimension = 0, sample = 0, access = 0, format = 0;
            if (!reader.Int(binding.group) || !reader.Int(binding.binding) || !reader.Int(binding.stages) || !reader.Int(kind) || !reader.Int(dimension) || !reader.Int(sample) || !reader.Int(access)
                || !reader.Int(format) || !reader.Int(binding.min_binding_size) || !reader.Int(binding.array_size) || !reader.String(binding.semantic) || !reader.String(binding.group_semantic)
                || !Vector(reader, binding.visible_entries, maximum, [&](std::string& entry) { return reader.String(entry); }) || !reader.String(binding.buffer_type)
                || !Vector(reader, binding.buffer_members, maximum, [&](BufferMemberLayout& member) { return read_member(member); }))
                return false;
            if (kind > static_cast<u8>(ResourceKind::ExternalTexture) || dimension > static_cast<u8>(TextureDimension::CubeArray) || sample > static_cast<u8>(SampleType::Depth)
                || access > static_cast<u8>(StorageAccess::ReadWrite) || format > static_cast<u8>(StorageFormat::RGBA32Float))
                return false;
            binding.kind = static_cast<ResourceKind>(kind);
            binding.dimension = static_cast<TextureDimension>(dimension);
            binding.sample_type = static_cast<SampleType>(sample);
            binding.storage_access = static_cast<StorageAccess>(access);
            binding.storage_format = static_cast<StorageFormat>(format);
            return true;
        }))
        return false;
    if (!Vector(reader, interface.overrides, maximum, [&](OverrideInfo& value) {
            u8 type = 0, has_default = 0;
            if (!reader.String(value.name) || !reader.Int(value.id) || !reader.Int(type) || !reader.Int(has_default) || type > static_cast<u8>(ValueType::Vec4U) || has_default > 1)
                return false;
            value.type = static_cast<ValueType>(type);
            value.has_default = has_default != 0;
            return true;
        }))
        return false;
    return Vector(reader, interface.capabilities, maximum, [&](std::string& value) { return reader.String(value); });
}

bool Fits(const ShaderPayload& payload, const ShaderPayloadLimits limits) {
    const auto valid_string = [&](const std::string_view value) { return value.size() <= limits.max_string_bytes && value.size() <= std::numeric_limits<u32>::max(); };
    if (payload.code.size() > limits.max_code_bytes || !valid_string(payload.code) || payload.dependencies.size() > limits.max_records || payload.source_map.size() > limits.max_records
        || payload.interface.entry_points.size() > limits.max_records || payload.interface.bindings.size() > limits.max_records || payload.interface.overrides.size() > limits.max_records
        || payload.interface.capabilities.size() > limits.max_records)
        return false;
    for (const auto& entry : payload.interface.entry_points)
        if (!valid_string(entry.name) || !valid_string(entry.semantic) || entry.inputs.size() > limits.max_records || entry.outputs.size() > limits.max_records)
            return false;
    std::function<bool(const BufferMemberLayout&)> valid_member;
    valid_member = [&](const BufferMemberLayout& member) {
        return valid_string(member.name) && valid_string(member.type) && member.members.size() <= limits.max_records && std::ranges::all_of(member.members, valid_member);
    };
    for (const auto& binding : payload.interface.bindings)
        if (!valid_string(binding.semantic) || !valid_string(binding.group_semantic) || !valid_string(binding.buffer_type) || binding.visible_entries.size() > limits.max_records
            || binding.buffer_members.size() > limits.max_records || std::ranges::any_of(binding.visible_entries, [&](const std::string& entry) { return !valid_string(entry); })
            || !std::ranges::all_of(binding.buffer_members, valid_member))
            return false;
    for (const auto& value : payload.interface.overrides)
        if (!valid_string(value.name))
            return false;
    for (const auto& capability : payload.interface.capabilities)
        if (!valid_string(capability))
            return false;
    for (const auto& path : payload.dependencies)
        if (!valid_string(path.String()))
            return false;
    for (const auto& map : payload.source_map)
        if (!valid_string(map.source.String()))
            return false;
    return true;
}

} // namespace

Result<std::vector<std::byte>> SerializeShaderPayload(const ShaderPayload& input, const ShaderPayloadLimits limits) {
    ShaderPayload payload = input;
    NormalizeInterface(payload.interface);
    if (!Fits(payload, limits))
        return Err(ErrorCode::OutOfRange, "shader payload exceeds configured limits");
    if (!std::ranges::is_sorted(payload.dependencies) || std::ranges::adjacent_find(payload.dependencies) != payload.dependencies.end())
        return Err(ErrorCode::ValidationInvalidState, "shader payload dependencies are not canonical");
    if (std::ranges::any_of(payload.interface.bindings, [](const BindingInfo& binding) { return binding.kind == ResourceKind::ExternalTexture; }))
        return Err(ErrorCode::GraphicsUnsupportedApi, "external-texture bindings are unsupported by the RHI layout model");
    if (payload.module_hash != Sha256(payload.code) || payload.interface_hash != payload.interface.hash)
        return Err(ErrorCode::ValidationInvalidState, "shader payload hashes do not match its contents");
    Writer writer;
    writer.bytes.insert(writer.bytes.end(), kMagic.begin(), kMagic.end());
    writer.Int(kShaderPayloadVersion);
    writer.Int(u32{0});
    writer.Hash(payload.module_hash);
    writer.Hash(payload.interface_hash);
    writer.Hash(payload.variant_hash);
    writer.String(payload.code);
    WriteInterface(writer, payload.interface);
    writer.Int(static_cast<u32>(payload.dependencies.size()));
    for (const auto& path : payload.dependencies)
        writer.String(path.String());
    writer.Int(static_cast<u32>(payload.source_map.size()));
    for (const auto& map : payload.source_map) {
        writer.Int(map.generated_begin);
        writer.Int(map.generated_end);
        writer.String(map.source.String());
        writer.Int(map.source_begin);
        writer.Int(map.source_end);
    }
    return Ok(std::move(writer.bytes));
}

Result<ShaderPayload> ParseShaderPayload(const std::span<const std::byte> bytes, const ShaderPayloadLimits limits) {
    if (bytes.size() < kMagic.size() || !std::ranges::equal(kMagic, bytes.first(kMagic.size())))
        return Err(ErrorCode::ParseInvalidFormat, "shader payload magic is invalid");
    Reader reader(bytes.subspan(kMagic.size()), limits);
    ShaderPayload payload;
    u32 version = 0, flags = 0;
    if (!reader.Int(version) || !reader.Int(flags) || version != kShaderPayloadVersion || flags != 0 || !reader.Hash(payload.module_hash) || !reader.Hash(payload.interface_hash) || !reader.Hash(payload.variant_hash)
        || !reader.String(payload.code) || payload.code.size() > limits.max_code_bytes || !ReadInterface(reader, payload.interface, limits.max_records))
        return Err(ErrorCode::ParseInvalidFormat, "shader payload header or interface is invalid");
    u32 dependency_count = 0;
    if (!reader.Int(dependency_count) || dependency_count > limits.max_records)
        return Err(ErrorCode::ParseInvalidFormat, "shader payload dependencies are invalid");
    payload.dependencies.reserve(dependency_count);
    for (u32 i = 0; i < dependency_count; ++i) {
        std::string text;
        if (!reader.String(text))
            return Err(ErrorCode::ParseInvalidFormat, "shader payload dependency is truncated");
        auto path = asset::AssetPath::Parse(text);
        if (!path)
            return Err(ErrorCode::ParseInvalidFormat, "shader payload dependency path is invalid");
        payload.dependencies.push_back(std::move(*path));
    }
    if (!std::ranges::is_sorted(payload.dependencies) || std::ranges::adjacent_find(payload.dependencies) != payload.dependencies.end())
        return Err(ErrorCode::ParseInvalidFormat, "shader payload dependencies are not canonical");
    u32 map_count = 0;
    if (!reader.Int(map_count) || map_count > limits.max_records)
        return Err(ErrorCode::ParseInvalidFormat, "shader payload source map is invalid");
    payload.source_map.reserve(map_count);
    for (u32 i = 0; i < map_count; ++i) {
        u64 generated_begin = 0, generated_end = 0, source_begin = 0, source_end = 0;
        std::string text;
        if (!reader.Int(generated_begin) || !reader.Int(generated_end) || !reader.String(text) || !reader.Int(source_begin) || !reader.Int(source_end))
            return Err(ErrorCode::ParseInvalidFormat, "shader payload source map is truncated");
        auto path = asset::AssetPath::Parse(text);
        if (!path || generated_begin > generated_end || source_begin > source_end || generated_end > payload.code.size())
            return Err(ErrorCode::ParseInvalidFormat, "shader payload source map range is invalid");
        payload.source_map.push_back({generated_begin, generated_end, std::move(*path), source_begin, source_end});
    }
    if (!reader.End())
        return Err(ErrorCode::ParseInvalidFormat, "shader payload contains trailing bytes");
    NormalizeInterface(payload.interface);
    if (std::ranges::any_of(payload.interface.bindings, [](const BindingInfo& binding) { return binding.kind == ResourceKind::ExternalTexture; }))
        return Err(ErrorCode::GraphicsUnsupportedApi, "shader payload uses unsupported external-texture bindings");
    if (payload.module_hash != Sha256(payload.code) || payload.interface_hash != payload.interface.hash)
        return Err(ErrorCode::ParseInvalidFormat, "shader payload hashes are invalid");
    return Ok(std::move(payload));
}

Result<asset::Product> MakeShaderProduct(const asset::AssetId asset_id, const ShaderPayload& payload, ContentHash source_hash, std::vector<asset::ProductDependency> dependencies) {
    auto bytes = SerializeShaderPayload(payload);
    if (!bytes)
        return Err(std::move(bytes).error());
    std::ranges::sort(dependencies);
    return Ok(asset::MakeProduct(asset_id, kShaderProductType, kShaderPayloadVersion, 1, std::move(source_hash), std::move(dependencies), Sha256("woki.gfx.generic-target"), std::move(*bytes)));
}

} // namespace woki::gfx
