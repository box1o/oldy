#include <set>
#include <charconv>
#include <woki/config.hpp>

#include <woki/gfx/advanced/descriptor.hpp>

namespace woki::gfx {
namespace {

using Json = config::Json;

void Add(std::vector<ShaderDiagnostic>& diagnostics, const asset::AssetPath& path, std::string code, std::string message, const u64 offset = 0) {
    diagnostics.push_back({std::move(code), DiagnosticSeverity::Error, std::move(message), SourceRange{path, offset, 1, 0, 0}, {}});
}

std::optional<ShaderStage> ParseStage(const std::string_view value) {
    if (value == "vertex")
        return ShaderStage::Vertex;
    if (value == "fragment")
        return ShaderStage::Fragment;
    if (value == "compute")
        return ShaderStage::Compute;
    return std::nullopt;
}

std::optional<PermutationValue> ParseValue(const Json& value) {
    if (value.is_boolean())
        return value.get<bool>();
    if (value.is_number_integer())
        return value.get<i64>();
    if (value.is_number_float())
        return value.get<f64>();
    if (value.is_string())
        return value.get<std::string>();
    return std::nullopt;
}

bool HasOnlyKeys(const Json& object, const std::initializer_list<std::string_view> keys) {
    return std::ranges::all_of(object.items(), [&](const auto& item) { return std::ranges::find(keys, item.first) != keys.end(); });
}

bool SemanticName(const std::string_view value) {
    if (value.empty() || (!(value.front() >= 'A' && value.front() <= 'Z') && !(value.front() >= 'a' && value.front() <= 'z') && value.front() != '_'))
        return false;
    return std::ranges::all_of(value.substr(1), [](const char c) { return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_'; });
}

std::optional<std::pair<u32, u32>> BindingKey(const std::string_view value) {
    const auto separator = value.find(':');
    if (separator == std::string_view::npos || separator == 0 || separator + 1 == value.size() || value.find(':', separator + 1) != std::string_view::npos || (separator > 1 && value.front() == '0')
        || (value.size() - separator - 1 > 1 && value[separator + 1] == '0'))
        return std::nullopt;
    u32 group{}, binding{};
    const auto group_result = std::from_chars(value.data(), value.data() + separator, group);
    const auto binding_result = std::from_chars(value.data() + separator + 1, value.data() + value.size(), binding);
    if (group_result.ec != std::errc{} || group_result.ptr != value.data() + separator || binding_result.ec != std::errc{} || binding_result.ptr != value.data() + value.size())
        return std::nullopt;
    return std::pair{group, binding};
}

} // namespace

DescriptorResult ParseShaderDescriptor(const asset::AssetPath& path, const std::string_view jsonc) {
    DescriptorResult result;
    auto parsed_root = Json::Parse(jsonc, path.String());
    if (!parsed_root) {
        const auto& error = parsed_root.error().front();
        Add(result.diagnostics, path, "SHD1001", error.message, error.range.begin.byte);
        return result;
    }
    Json root = std::move(*parsed_root);
    if (!root.is_object()) {
        Add(result.diagnostics, path, "SHD1002", "shader descriptor root must be an object");
        return result;
    }
    {
        if (!HasOnlyKeys(root, {"$schema", "schema", "name", "language", "sources", "entry_points", "permutations", "capabilities", "bindings", "groups", "compile_options"}))
            Add(result.diagnostics, path, "SHD1020", "shader descriptor contains an unsupported key");

        const auto schema = root.find("schema");
        if (schema == root.end() || !schema->is_number_unsigned() || (*schema != 1 && *schema != kShaderDescriptorSchema)) {
            Add(result.diagnostics, path, "SHD1003", "descriptor schema must be 1 or 2");
        } else {
            // v1 has no semantic fields and migrates losslessly to the v2 in-memory model.
            result.descriptor.schema = kShaderDescriptorSchema;
        }
        const auto name = root.find("name");
        if (name == root.end() || !name->is_string() || name->get_ref<const std::string&>().empty()) {
            Add(result.diagnostics, path, "SHD1004", "descriptor name must be a non-empty string");
        } else {
            result.descriptor.name = name->get<std::string>();
        }
        const auto language = root.find("language");
        if (language == root.end() || !language->is_string() || language->get<std::string>() != "wgsl") {
            Add(result.diagnostics, path, "SHD1005", "descriptor language must be 'wgsl'");
        }

        const auto sources = root.find("sources");
        if (sources == root.end() || !sources->is_array() || sources->empty()) {
            Add(result.diagnostics, path, "SHD1006", "descriptor sources must be a non-empty array");
        } else {
            for (const Json& source : *sources) {
                if (!source.is_string()) {
                    Add(result.diagnostics, path, "SHD1007", "source paths must be strings");
                    continue;
                }
                auto parsed = asset::AssetPath::Parse(source.get_ref<const std::string&>());
                if (!parsed)
                    Add(result.diagnostics, path, "SHD1008", "source path is not a safe virtual path");
                else
                    result.descriptor.sources.push_back(std::move(*parsed));
            }
        }

        const auto entries = root.find("entry_points");
        std::set<std::pair<ShaderStage, std::string>> unique_entries;
        if (entries == root.end() || !entries->is_array() || entries->empty()) {
            Add(result.diagnostics, path, "SHD1009", "entry_points must be a non-empty array");
        } else {
            for (const Json& entry : *entries) {
                if (!entry.is_object() || !HasOnlyKeys(entry, {"stage", "name", "semantic"}) || !entry.contains("stage") || !entry.contains("name") || !entry["stage"].is_string() || !entry["name"].is_string()
                    || (entry.contains("semantic") && !entry["semantic"].is_string())) {
                    Add(result.diagnostics, path, "SHD1010", "entry point requires string stage and name");
                    continue;
                }
                const auto stage = ParseStage(entry["stage"].get_ref<const std::string&>());
                const std::string entry_name = entry["name"].get<std::string>();
                if (!stage || !SemanticName(entry_name)) {
                    Add(result.diagnostics, path, "SHD1011", "entry point stage or name is invalid");
                } else if (!unique_entries.emplace(*stage, entry_name).second) {
                    Add(result.diagnostics, path, "SHD1012", "entry point is duplicated");
                } else {
                    const std::string semantic = entry.value("semantic", entry_name);
                    if (!SemanticName(semantic))
                        Add(result.diagnostics, path, "SHD1026", "entry point semantic must be an identifier");
                    else
                        result.descriptor.entry_points.push_back({*stage, entry_name, semantic});
                }
            }
        }

        const auto permutations = root.find("permutations");
        std::set<std::string, std::less<>> permutation_names;
        if (permutations != root.end()) {
            if (!permutations->is_array()) {
                Add(result.diagnostics, path, "SHD1013", "permutations must be an array");
            } else
                for (const Json& permutation : *permutations) {
                    if (!permutation.is_object() || !HasOnlyKeys(permutation, {"name", "values"}) || !permutation.contains("name") || !permutation.contains("values") || !permutation["name"].is_string()
                        || !permutation["values"].is_array()) {
                        Add(result.diagnostics, path, "SHD1014", "permutation requires string name and values array");
                        continue;
                    }
                    PermutationDesc domain{permutation["name"].get<std::string>(), {}};
                    if (domain.name.empty() || !permutation_names.insert(domain.name).second || permutation["values"].empty()) {
                        Add(result.diagnostics, path, "SHD1015", "permutation names must be unique and domains non-empty");
                        continue;
                    }
                    for (const Json& value : permutation["values"]) {
                        auto parsed = ParseValue(value);
                        if (!parsed)
                            Add(result.diagnostics, path, "SHD1016", "permutation values must be scalar");
                        else if (std::ranges::find(domain.values, *parsed) != domain.values.end())
                            Add(result.diagnostics, path, "SHD1025", "permutation values must be unique");
                        else
                            domain.values.push_back(std::move(*parsed));
                    }
                    result.descriptor.permutations.push_back(std::move(domain));
                }
        }

        const auto capabilities = root.find("capabilities");
        if (capabilities != root.end()) {
            if (!capabilities->is_array())
                Add(result.diagnostics, path, "SHD1017", "capabilities must be an array");
            else
                for (const Json& capability : *capabilities) {
                    if (!capability.is_string())
                        Add(result.diagnostics, path, "SHD1018", "capabilities must be strings");
                    else
                        result.descriptor.capabilities.push_back(capability.get<std::string>());
                }
        }
        const auto bindings = root.find("bindings");
        if (bindings != root.end()) {
            if (!bindings->is_object()) {
                Add(result.diagnostics, path, "SHD1027", "bindings must be an object keyed by 'group:binding'");
            } else {
                for (const auto& [key, value] : bindings->items()) {
                    const auto parsed = BindingKey(key);
                    if (!parsed || !value.is_string() || !SemanticName(value.get_ref<const std::string&>()) || !result.descriptor.binding_semantics.emplace(*parsed, value.get<std::string>()).second)
                        Add(result.diagnostics, path, "SHD1028", "binding semantics require unique canonical numeric keys and identifier values");
                }
            }
        }
        const auto groups = root.find("groups");
        if (groups != root.end()) {
            if (!groups->is_object()) {
                Add(result.diagnostics, path, "SHD1029", "groups must be an object keyed by a canonical group number");
            } else {
                for (const auto& [key, value] : groups->items()) {
                    u32 group{};
                    const auto parsed = std::from_chars(key.data(), key.data() + key.size(), group);
                    if (parsed.ec != std::errc{} || parsed.ptr != key.data() + key.size() || (key.size() > 1 && key.front() == '0') || !value.is_string() || !SemanticName(value.get_ref<const std::string&>())
                        || !result.descriptor.group_semantics.emplace(group, value.get<std::string>()).second)
                        Add(result.diagnostics, path, "SHD1030", "group semantics require unique canonical numeric keys and identifier values");
                }
            }
        }
        std::set<std::string, std::less<>> binding_semantic_names;
        for (const auto& [coordinate, semantic] : result.descriptor.binding_semantics) {
            static_cast<void>(coordinate);
            if (!binding_semantic_names.insert(semantic).second)
                Add(result.diagnostics, path, "SHD1031", "binding semantic names must be unique");
        }
        std::set<std::string, std::less<>> entry_semantic_names;
        for (const auto& entry : result.descriptor.entry_points)
            if (!entry_semantic_names.insert(entry.semantic).second)
                Add(result.diagnostics, path, "SHD1032", "selected entry semantic names must be unique");
        const auto options = root.find("compile_options");
        if (options != root.end()) {
            if (!options->is_object())
                Add(result.diagnostics, path, "SHD1019", "compile_options must be an object");
            else {
                if (!HasOnlyKeys(*options, {"warnings_as_errors", "emit_source_map"}))
                    Add(result.diagnostics, path, "SHD1021", "compile_options contains an unsupported key");
                if (options->contains("warnings_as_errors")) {
                    if ((*options)["warnings_as_errors"].is_boolean())
                        result.descriptor.compile_options.warnings_as_errors = (*options)["warnings_as_errors"].get<bool>();
                    else
                        Add(result.diagnostics, path, "SHD1022", "warnings_as_errors must be boolean");
                }
                if (options->contains("emit_source_map")) {
                    if ((*options)["emit_source_map"].is_boolean())
                        result.descriptor.compile_options.emit_source_map = (*options)["emit_source_map"].get<bool>();
                    else
                        Add(result.diagnostics, path, "SHD1023", "emit_source_map must be boolean");
                }
            }
        }
        std::ranges::sort(result.descriptor.capabilities);
        result.descriptor.capabilities.erase(std::ranges::unique(result.descriptor.capabilities).begin(), result.descriptor.capabilities.end());
    }
    return result;
}

} // namespace woki::gfx
