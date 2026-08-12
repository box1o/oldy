#include <woki/gfx/pack.hpp>

#include <algorithm>
#include <cctype>
#include <cstring>
#include <limits>
#include <set>

#include <nlohmann/json.hpp>

namespace woki::gfx {
namespace {

using Json = nlohmann::json;
constexpr std::array<std::byte, 8> kMagic{std::byte{'W'}, std::byte{'S'}, std::byte{'P'}, std::byte{'I'}, std::byte{'D'}, std::byte{'X'}, std::byte{0}, std::byte{1}};

Result<asset::AssetPath> RelativeTo(const asset::AssetPath& parent, const std::string_view child) {
    if (child.empty() || child.front() == '/' || child.find('\\') != std::string_view::npos || child.find(':') != std::string_view::npos)
        return Err(ErrorCode::InvalidArgument, "pack path must be relative");
    const auto slash = parent.String().rfind('/');
    return asset::AssetPath::Parse(slash == std::string::npos ? std::string(child) : parent.String().substr(0, slash + 1) + std::string(child));
}

void CanonicalString(std::string& output, const std::string_view value) {
    output += std::to_string(value.size()) + ":" + std::string(value);
}

void U32(std::vector<std::byte>& output, const u32 value) {
    for (u32 shift = 0; shift != 32; shift += 8)
        output.push_back(static_cast<std::byte>((value >> shift) & 0xffU));
}

Result<void> String(std::vector<std::byte>& output, const std::string_view value) {
    if (value.size() > std::numeric_limits<u32>::max())
        return Err(ErrorCode::OutOfRange, "shader pack index string is too large");
    U32(output, static_cast<u32>(value.size()));
    const auto* begin = reinterpret_cast<const std::byte*>(value.data());
    output.insert(output.end(), begin, begin + value.size());
    return Ok();
}

class Reader {
public:
    explicit Reader(const std::span<const std::byte> bytes)
        : bytes_(bytes) {}

    Result<u32> U32Value() {
        if (bytes_.size() - offset_ < 4)
            return Err(ErrorCode::ParseInvalidFormat, "shader pack index is truncated");
        u32 value = 0;
        for (u32 shift = 0; shift != 32; shift += 8)
            value |= std::to_integer<u32>(bytes_[offset_++]) << shift;
        return Ok(value);
    }

    Result<std::string> StringValue() {
        u32 size = 0;
        TRY_ASSIGN(size, U32Value());
        if (size > bytes_.size() - offset_)
            return Err(ErrorCode::ParseInvalidFormat, "shader pack index string is truncated");
        std::string value(size, '\0');
        std::memcpy(value.data(), bytes_.data() + offset_, size);
        offset_ += size;
        return Ok(std::move(value));
    }

    Result<ContentHash> Hash() {
        if (bytes_.size() - offset_ < ContentHash::kSize)
            return Err(ErrorCode::ParseInvalidFormat, "shader pack index hash is truncated");
        std::array<u8, ContentHash::kSize> value{};
        std::memcpy(value.data(), bytes_.data() + offset_, value.size());
        offset_ += value.size();
        return Ok(ContentHash(value));
    }

    [[nodiscard]] bool Done() const noexcept {
        return offset_ == bytes_.size();
    }

private:
    std::span<const std::byte> bytes_;
    std::size_t offset_{0};
};

bool ValidIdentity(const std::string_view value) {
    return !value.empty() && std::ranges::all_of(value, [](const unsigned char c) { return std::isalnum(c) || c == '.' || c == '-' || c == '_'; });
}

bool HasOnlyKeys(const Json& object, const std::initializer_list<std::string_view> keys) {
    return std::ranges::all_of(object.items(), [&](const auto& item) { return std::ranges::find(keys, item.key()) != keys.end(); });
}

ContentHash AggregateHash(const ShaderPackIndex& index) {
    std::string aggregate;
    CanonicalString(aggregate, index.id);
    CanonicalString(aggregate, index.version);
    for (const auto& dependency : index.dependencies) {
        CanonicalString(aggregate, dependency.id);
        CanonicalString(aggregate, dependency.version);
    }
    for (const auto& entry : index.entries) {
        CanonicalString(aggregate, entry.name);
        CanonicalString(aggregate, entry.descriptor_path.String());
        CanonicalString(aggregate, entry.content_hash.Hex());
        for (const auto& dependency : entry.dependencies)
            CanonicalString(aggregate, dependency.String());
    }
    return Sha256(aggregate);
}

bool IsCanonical(const ShaderPackIndex& index) {
    if (!ValidIdentity(index.id) || !ValidIdentity(index.version) || !std::ranges::is_sorted(index.dependencies) || std::ranges::adjacent_find(index.dependencies) != index.dependencies.end()
        || !std::ranges::is_sorted(index.entries, {}, &ShaderPackIndexEntry::name) || std::ranges::adjacent_find(index.entries, {}, &ShaderPackIndexEntry::name) != index.entries.end())
        return false;
    if (std::ranges::any_of(index.dependencies, [](const auto& dependency) { return !ValidIdentity(dependency.id) || !ValidIdentity(dependency.version); }))
        return false;
    return std::ranges::all_of(index.entries, [](const auto& entry) {
        return ValidIdentity(entry.name) && std::ranges::is_sorted(entry.dependencies) && std::ranges::adjacent_find(entry.dependencies) == entry.dependencies.end()
               && std::ranges::binary_search(entry.dependencies, entry.descriptor_path);
    });
}

} // namespace

Result<ShaderPack> LoadShaderPack(const asset::Vfs& vfs, const asset::AssetPath& manifest_path) {
    auto text = vfs.ReadText(manifest_path);
    if (!text)
        return Err(std::move(text).error());
    Json root;
    try {
        root = Json::parse(*text, nullptr, true, true);
    } catch (const Json::exception&) {
        return Err(ErrorCode::ParseInvalidFormat, "shader pack manifest is not valid JSONC");
    }
    try {
        if (!root.is_object() || !HasOnlyKeys(root, {"schema", "id", "version", "dependencies", "shaders"}) || !root.contains("schema") || !root["schema"].is_number_unsigned()
            || root["schema"].get<u32>() != kShaderPackSchema || !root.contains("id") || !root["id"].is_string() || !root.contains("version") || !root["version"].is_string() || !root.contains("shaders")
            || !root["shaders"].is_array())
            return Err(ErrorCode::ParseInvalidFormat, "shader pack manifest requires schema 1, id, version, and shaders");

        ShaderPack pack{.id = root["id"].get<std::string>(), .version = root["version"].get<std::string>(), .dependencies = {}, .entries = {}, .content_hash = {}};
        if (!ValidIdentity(pack.id) || !ValidIdentity(pack.version))
            return Err(ErrorCode::ParseInvalidFormat, "shader pack id and version must be stable identifiers");
        if (root.contains("dependencies")) {
            if (!root["dependencies"].is_array())
                return Err(ErrorCode::ParseInvalidFormat, "shader pack dependencies must be an array");
            for (const auto& dependency : root["dependencies"]) {
                if (!dependency.is_object() || !HasOnlyKeys(dependency, {"id", "version"}) || !dependency.contains("id") || !dependency["id"].is_string() || !dependency.contains("version")
                    || !dependency["version"].is_string())
                    return Err(ErrorCode::ParseInvalidFormat, "shader pack dependency requires id and version");
                pack.dependencies.push_back({dependency["id"].get<std::string>(), dependency["version"].get<std::string>()});
                if (!ValidIdentity(pack.dependencies.back().id) || !ValidIdentity(pack.dependencies.back().version))
                    return Err(ErrorCode::ParseInvalidFormat, "shader pack dependency id and version must be stable identifiers");
            }
        }
        std::ranges::sort(pack.dependencies);
        if (std::ranges::adjacent_find(pack.dependencies) != pack.dependencies.end())
            return Err(ErrorCode::ParseInvalidFormat, "shader pack dependency is duplicated");

        std::set<std::string, std::less<>> names;
        std::set<asset::AssetPath> paths;
        for (const auto& shader : root["shaders"]) {
            if (!shader.is_object() || !HasOnlyKeys(shader, {"name", "descriptor"}) || !shader.contains("name") || !shader["name"].is_string() || !shader.contains("descriptor") || !shader["descriptor"].is_string())
                return Err(ErrorCode::ParseInvalidFormat, "shader pack entry requires name and descriptor");
            const std::string entry_name = shader["name"].get<std::string>();
            if (!ValidIdentity(entry_name) || !names.insert(entry_name).second)
                return Err(ErrorCode::ParseInvalidFormat, "shader pack entry name is invalid or duplicated");
            auto path = RelativeTo(manifest_path, shader["descriptor"].get_ref<const std::string&>());
            if (!path || !paths.insert(*path).second)
                return Err(ErrorCode::ParseInvalidFormat, "shader pack descriptor path is invalid or duplicated");
            ShaderPackEntry entry{.name = entry_name, .descriptor_path = std::move(*path), .descriptor = {}, .source = {}, .content_hash = {}};
            auto descriptor_text = vfs.ReadText(entry.descriptor_path);
            if (!descriptor_text)
                return Err(std::move(descriptor_text).error());
            auto parsed = ParseShaderDescriptor(entry.descriptor_path, *descriptor_text);
            if (std::ranges::any_of(parsed.diagnostics, [](const auto& diagnostic) { return diagnostic.severity == DiagnosticSeverity::Error; }))
                return Err(ErrorCode::ParseInvalidFormat, "shader pack contains an invalid descriptor");
            entry.descriptor = std::move(parsed.descriptor);
            entry.source = ShaderSourceResolver(vfs).Compose(entry.descriptor, entry.descriptor_path);
            if (std::ranges::any_of(entry.source.diagnostics, [](const auto& diagnostic) { return diagnostic.severity == DiagnosticSeverity::Error; }))
                return Err(ErrorCode::ParseInvalidFormat, "shader pack source composition failed");

            std::string content;
            CanonicalString(content, entry.name);
            CanonicalString(content, entry.descriptor_path.String());
            CanonicalString(content, *descriptor_text);
            for (const auto& dependency : entry.source.dependencies) {
                auto source = vfs.ReadText(dependency);
                if (!source)
                    return Err(std::move(source).error());
                CanonicalString(content, dependency.String());
                CanonicalString(content, *source);
            }
            entry.content_hash = Sha256(content);
            pack.entries.push_back(std::move(entry));
        }
        if (pack.entries.empty())
            return Err(ErrorCode::ParseInvalidFormat, "shader pack must declare at least one shader");
        std::ranges::sort(pack.entries, {}, &ShaderPackEntry::name);

        ShaderPackIndex index = MakeShaderPackIndex(pack);
        pack.content_hash = AggregateHash(index);
        return Ok(std::move(pack));
    } catch (const Json::exception&) {
        return Err(ErrorCode::ParseInvalidFormat, "shader pack manifest contains an invalid value type");
    }
}

ShaderPackIndex MakeShaderPackIndex(const ShaderPack& pack) {
    ShaderPackIndex index{.id = pack.id, .version = pack.version, .dependencies = pack.dependencies, .entries = {}, .content_hash = {}};
    std::ranges::sort(index.dependencies);
    index.dependencies.erase(std::ranges::unique(index.dependencies).begin(), index.dependencies.end());
    for (const auto& entry : pack.entries) {
        auto dependencies = entry.source.dependencies;
        dependencies.push_back(entry.descriptor_path);
        std::ranges::sort(dependencies);
        dependencies.erase(std::ranges::unique(dependencies).begin(), dependencies.end());
        index.entries.push_back({entry.name, entry.descriptor_path, entry.content_hash, std::move(dependencies)});
    }
    std::ranges::sort(index.entries, {}, &ShaderPackIndexEntry::name);
    index.content_hash = AggregateHash(index);
    return index;
}

Result<std::vector<std::byte>> SerializeShaderPackIndex(const ShaderPackIndex& index) {
    if (index.dependencies.size() > std::numeric_limits<u32>::max() || index.entries.size() > std::numeric_limits<u32>::max()
        || std::ranges::any_of(index.entries, [](const auto& entry) { return entry.dependencies.size() > std::numeric_limits<u32>::max(); }))
        return Err(ErrorCode::OutOfRange, "shader pack index has too many records");
    if (!IsCanonical(index) || index.content_hash != AggregateHash(index))
        return Err(ErrorCode::ValidationInvalidState, "shader pack index is not canonical or its aggregate hash is stale");
    std::vector<std::byte> output(kMagic.begin(), kMagic.end());
    U32(output, kShaderPackIndexVersion);
    TRY_VOID(String(output, index.id));
    TRY_VOID(String(output, index.version));
    output.insert(output.end(), reinterpret_cast<const std::byte*>(index.content_hash.Bytes().data()), reinterpret_cast<const std::byte*>(index.content_hash.Bytes().data() + index.content_hash.Bytes().size()));
    U32(output, static_cast<u32>(index.dependencies.size()));
    for (const auto& dependency : index.dependencies) {
        TRY_VOID(String(output, dependency.id));
        TRY_VOID(String(output, dependency.version));
    }
    U32(output, static_cast<u32>(index.entries.size()));
    for (const auto& entry : index.entries) {
        TRY_VOID(String(output, entry.name));
        TRY_VOID(String(output, entry.descriptor_path.String()));
        output.insert(output.end(), reinterpret_cast<const std::byte*>(entry.content_hash.Bytes().data()), reinterpret_cast<const std::byte*>(entry.content_hash.Bytes().data() + entry.content_hash.Bytes().size()));
        U32(output, static_cast<u32>(entry.dependencies.size()));
        for (const auto& dependency : entry.dependencies)
            TRY_VOID(String(output, dependency.String()));
    }
    return Ok(std::move(output));
}

Result<ShaderPackIndex> ParseShaderPackIndex(const std::span<const std::byte> bytes) {
    if (bytes.size() < kMagic.size() || !std::ranges::equal(bytes.first(kMagic.size()), kMagic))
        return Err(ErrorCode::ParseInvalidFormat, "shader pack index magic is invalid");
    Reader reader(bytes.subspan(kMagic.size()));
    u32 version = 0;
    TRY_ASSIGN(version, reader.U32Value());
    if (version != kShaderPackIndexVersion)
        return Err(ErrorCode::ParseInvalidFormat, "shader pack index version is unsupported");
    ShaderPackIndex index;
    TRY_ASSIGN(index.id, reader.StringValue());
    TRY_ASSIGN(index.version, reader.StringValue());
    TRY_ASSIGN(index.content_hash, reader.Hash());
    u32 dependency_count = 0;
    TRY_ASSIGN(dependency_count, reader.U32Value());
    if (dependency_count > 4096)
        return Err(ErrorCode::OutOfRange, "shader pack index dependency limit exceeded");
    for (u32 i = 0; i < dependency_count; ++i) {
        ShaderPackDependency dependency;
        TRY_ASSIGN(dependency.id, reader.StringValue());
        TRY_ASSIGN(dependency.version, reader.StringValue());
        index.dependencies.push_back(std::move(dependency));
    }
    u32 entry_count = 0;
    TRY_ASSIGN(entry_count, reader.U32Value());
    if (entry_count > 65536)
        return Err(ErrorCode::OutOfRange, "shader pack index entry limit exceeded");
    for (u32 i = 0; i < entry_count; ++i) {
        std::string name;
        TRY_ASSIGN(name, reader.StringValue());
        std::string path;
        TRY_ASSIGN(path, reader.StringValue());
        auto parsed_path = asset::AssetPath::Parse(path);
        if (!parsed_path)
            return Err(ErrorCode::ParseInvalidFormat, "shader pack index contains an invalid path");
        ContentHash content_hash;
        TRY_ASSIGN(content_hash, reader.Hash());
        ShaderPackIndexEntry entry{.name = std::move(name), .descriptor_path = std::move(*parsed_path), .content_hash = content_hash, .dependencies = {}};
        u32 path_count = 0;
        TRY_ASSIGN(path_count, reader.U32Value());
        if (path_count > 65536)
            return Err(ErrorCode::OutOfRange, "shader pack index path limit exceeded");
        for (u32 j = 0; j < path_count; ++j) {
            TRY_ASSIGN(path, reader.StringValue());
            parsed_path = asset::AssetPath::Parse(path);
            if (!parsed_path)
                return Err(ErrorCode::ParseInvalidFormat, "shader pack index contains an invalid dependency path");
            entry.dependencies.push_back(std::move(*parsed_path));
        }
        index.entries.push_back(std::move(entry));
    }
    if (!reader.Done() || !IsCanonical(index))
        return Err(ErrorCode::ParseInvalidFormat, "shader pack index is not canonical");
    if (AggregateHash(index) != index.content_hash)
        return Err(ErrorCode::ParseInvalidFormat, "shader pack index aggregate hash is invalid");
    return Ok(std::move(index));
}

} // namespace woki::gfx
