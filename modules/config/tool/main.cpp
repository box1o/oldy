#include <fstream>
#include <iostream>

#include <woki/config.hpp>

namespace {

int Diagnostics(const std::vector<woki::config::Diagnostic>& diagnostics) {
    std::cerr << woki::config::FormatDiagnostics(diagnostics);
    return 1;
}

std::optional<std::pair<std::string, woki::u32>> Id(std::string_view text) {
    const auto at = text.rfind('@');
    if (at == std::string_view::npos)
        return {};
    woki::u32 version{};
    const auto result = std::from_chars(text.data() + at + 1, text.data() + text.size(), version);
    if (result.ec != std::errc{} || result.ptr != text.data() + text.size())
        return {};
    return std::pair{std::string(text.substr(0, at)), version};
}

const woki::config::Value* Pointer(const woki::config::Value& root, std::string_view pointer) {
    const woki::config::Value* value = &root;
    while (!pointer.empty()) {
        if (pointer.front() != '/')
            return nullptr;
        pointer.remove_prefix(1);
        const auto slash = pointer.find('/');
        std::string token(pointer.substr(0, slash));
        pointer = slash == std::string_view::npos ? std::string_view{} : pointer.substr(slash);
        for (size_t pos = 0; (pos = token.find('~', pos)) != std::string::npos;) {
            if (pos + 1 >= token.size())
                return nullptr;
            if (token[pos + 1] == '0')
                token.replace(pos, 2, "~");
            else if (token[pos + 1] == '1')
                token.replace(pos, 2, "/");
            else
                return nullptr;
            ++pos;
        }
        if (const auto* members = value->Members()) {
            const auto found = std::ranges::find(*members, token, &woki::config::Member::key);
            if (found == members->end())
                return nullptr;
            value = found->value.get();
        } else if (const auto* elements = value->Elements()) {
            size_t index{};
            const auto parsed = std::from_chars(token.data(), token.data() + token.size(), index);
            if (parsed.ec != std::errc{} || parsed.ptr != token.data() + token.size() || index >= elements->size())
                return nullptr;
            value = (*elements)[index].get();
        } else
            return nullptr;
    }
    return value;
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "usage: woki-config <validate|migrate|upgrade|canonicalize|schema|get> ...\n";
        return 2;
    }
    const std::string_view command = argv[1];
    auto& registry = woki::config::Registry::Global();
    if (command == "schema") {
        if (argc < 3 || argc > 4)
            return 2;
        const auto emit = [&](const std::string& text) {
            if (argc == 3) {
                std::cout << text << '\n';
                return bool(std::cout);
            }
            std::ofstream output(argv[3], std::ios::binary | std::ios::trunc);
            output << text << '\n';
            return bool(output);
        };
        if (std::string_view(argv[2]) == "catalog")
            return emit(registry.Catalog()) ? 0 : 1;
        auto id = Id(argv[2]);
        if (!id)
            return 2;
        std::string schema = registry.JsonSchema(id->first, id->second);
        if (schema.empty())
            return 1;
        return emit(schema) ? 0 : 1;
    }
    if (argc < 3)
        return 2;
    const std::filesystem::path input_path(argv[2]);
    const auto json_policy = command == "migrate" || command == "upgrade" ? woki::config::ParsePolicy{true, true, false, {}} : woki::config::ParsePolicy::Authored();
    auto document = input_path.extension() == ".yaml" || input_path.extension() == ".yml" ? woki::config::Document::ParseYamlFile(input_path) : woki::config::Document::ParseFile(input_path, json_policy);
    if (!document)
        return Diagnostics(document.error());
    if (command == "canonicalize") {
        std::cout << woki::config::CanonicalJson(document->Root()) << '\n';
        return 0;
    }
    if (command == "get") {
        if (argc != 4)
            return 2;
        const auto* value = Pointer(document->Root(), argv[3]);
        if (!value)
            return 1;
        std::cout << woki::config::CanonicalJson(*value) << '\n';
        return 0;
    }
    if (argc != 4)
        return 2;
    auto id = Id(argv[3]);
    if (!id)
        return 2;
    if (command == "validate") {
        if (!registry.Maintained(id->first, id->second)) {
            std::cerr << "schema version is outside the maintained migration window\n";
            return 1;
        }
        const auto* schema = registry.Find(id->first, id->second);
        if (!schema)
            return 2;
        auto diagnostics = woki::config::Validate(*document, *schema);
        return diagnostics.empty() ? 0 : Diagnostics(diagnostics);
    }
    if (command == "migrate" || command == "upgrade") {
        woki::config::ObjectView root(document->Root());
        auto old = root.Unsigned("schema");
        const woki::u32 from = old ? static_cast<woki::u32>(*old) : id->second;
        auto migrated = registry.Migrate(*document, id->first, from, id->second);
        if (!migrated)
            return Diagnostics(migrated.error());
        std::cout << woki::config::CanonicalJson(migrated->Root()) << '\n';
        return 0;
    }
    return 2;
}
