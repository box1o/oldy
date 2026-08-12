#include <woki/gfx/descriptor.hpp>

#include <set>

#include <nlohmann/json.hpp>

namespace woki::gfx {
namespace {

using Json = nlohmann::json;

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
    return std::ranges::all_of(object.items(), [&](const auto& item) { return std::ranges::find(keys, item.key()) != keys.end(); });
}

} // namespace

DescriptorResult ParseShaderDescriptor(const asset::AssetPath& path, const std::string_view jsonc) {
    DescriptorResult result;
    Json root;
    try {
        root = Json::parse(jsonc, nullptr, true, true);
    } catch (const Json::parse_error& error) {
        Add(result.diagnostics, path, "SHD1001", error.what(), error.byte > 0 ? error.byte - 1 : 0);
        return result;
    }
    if (!root.is_object()) {
        Add(result.diagnostics, path, "SHD1002", "shader descriptor root must be an object");
        return result;
    }
    try {
        if (!HasOnlyKeys(root, {"schema", "name", "language", "sources", "entry_points", "permutations", "capabilities", "compile_options"}))
            Add(result.diagnostics, path, "SHD1020", "shader descriptor contains an unsupported key");

        const auto schema = root.find("schema");
        if (schema == root.end() || !schema->is_number_unsigned() || *schema != kShaderDescriptorSchema) {
            Add(result.diagnostics, path, "SHD1003", "descriptor schema must be 1");
        } else {
            result.descriptor.schema = schema->get<u32>();
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
                if (!entry.is_object() || !HasOnlyKeys(entry, {"stage", "name"}) || !entry.contains("stage") || !entry.contains("name") || !entry["stage"].is_string() || !entry["name"].is_string()) {
                    Add(result.diagnostics, path, "SHD1010", "entry point requires string stage and name");
                    continue;
                }
                const auto stage = ParseStage(entry["stage"].get_ref<const std::string&>());
                const std::string entry_name = entry["name"].get<std::string>();
                if (!stage || entry_name.empty()) {
                    Add(result.diagnostics, path, "SHD1011", "entry point stage or name is invalid");
                } else if (!unique_entries.emplace(*stage, entry_name).second) {
                    Add(result.diagnostics, path, "SHD1012", "entry point is duplicated");
                } else {
                    result.descriptor.entry_points.push_back({*stage, entry_name});
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
    } catch (const Json::exception&) {
        Add(result.diagnostics, path, "SHD1024", "shader descriptor contains a value outside its supported type or range");
    }
    return result;
}

} // namespace woki::gfx
