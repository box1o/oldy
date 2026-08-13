#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

#include <woki/config.hpp>

#include <woki/gfx/advanced.hpp>

namespace {
using namespace woki;

Result<std::string> ReadText(const std::filesystem::path& path) {
    std::ifstream stream(path);
    if (!stream)
        return Err(ErrorCode::FileNotFound, "unable to open material source");
    std::ostringstream text;
    text << stream.rdbuf();
    if (!stream && !stream.eof())
        return Err(ErrorCode::FileReadError, "unable to read material source");
    return Ok(text.str());
}

Result<asset::Product> ReadProduct(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary | std::ios::ate);
    if (!stream)
        return Err(ErrorCode::FileNotFound, "unable to open dependency product");
    const auto end = stream.tellg();
    if (end < 0)
        return Err(ErrorCode::FileReadError, "unable to determine dependency product size");
    std::vector<std::byte> bytes(static_cast<size_t>(end));
    stream.seekg(0);
    if (!bytes.empty() && !stream.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size())))
        return Err(ErrorCode::FileReadError, "unable to read dependency product");
    return asset::ParseProduct(bytes);
}

Result<void> WriteProduct(const std::filesystem::path& path, const asset::Product& product) {
    std::vector<std::byte> bytes;
    TRY_ASSIGN(bytes, asset::SerializeProduct(product));
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    if (!stream || (!bytes.empty() && !stream.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()))))
        return Err(ErrorCode::FileWriteError, "unable to write cooked material product");
    return Ok();
}

int Cook(const bool type, const std::filesystem::path& source_path, const std::filesystem::path& dependency_path, const std::filesystem::path& output_path) {
    auto text = ReadText(source_path);
    auto dependency = ReadProduct(dependency_path);
    if (!text || !dependency) {
        std::cerr << (!text ? text.error().Message() : dependency.error().Message()) << '\n';
        return 1;
    }
    asset::AssetId id;
    if (type) {
        auto source = gfx::ParseMaterialType(*text);
        if (!source) {
            std::cerr << source.error().Message() << '\n';
            return 1;
        }
        id = source->id;
    } else {
        auto parsed_root = config::Json::Parse(*text, source_path.string());
        if (!parsed_root) {
            std::cerr << config::FormatDiagnostics(parsed_root.error()) << '\n';
            return 1;
        }
        const config::Json root = std::move(*parsed_root);
        if (!root.contains("id") || !root["id"].is_string()) {
            std::cerr << "material instance id is missing\n";
            return 1;
        }
        auto parsed = asset::AssetId::Parse(root["id"].get_ref<const std::string&>());
        if (!parsed) {
            std::cerr << parsed.error().Message() << '\n';
            return 1;
        }
        id = *parsed;
    }
    const auto resolve = [product = *dependency](const asset::AssetId requested) -> Result<asset::Product> {
        return requested == product.asset_id ? Ok(product) : Result<asset::Product>(Err(ErrorCode::FileNotFound, "material dependency product was not supplied"));
    };
    asset::BuildContext context;
    const auto uri = asset::AssetUri::Parse("engine://materials/cli-source");
    if (!uri)
        return 1;
    asset::Product product;
    auto document = config::Document::Parse(*text, source_path.string(), config::ParsePolicy{true, true, false, {}});
    const ContentHash source_hash = document ? config::CanonicalHash(*document, type ? "material-type" : "material", 1) : Sha256(*text);
    const asset::BuildRequest request{id, *uri, source_hash, {}, 2};
    auto built = type ? gfx::MaterialTypeBuilder(resolve).Build(request, context, std::as_bytes(std::span(*text))) : gfx::MaterialInstanceBuilder(resolve).Build(request, context, std::as_bytes(std::span(*text)));
    if (!built) {
        std::cerr << built.error().Message() << '\n';
        return 1;
    }
    if (auto written = WriteProduct(output_path, *built); !written) {
        std::cerr << written.error().Message() << '\n';
        return 1;
    }
    return 0;
}

int ValidateType(const std::filesystem::path& path) {
    auto text = ReadText(path);
    if (!text) {
        std::cerr << text.error().Message() << '\n';
        return 1;
    }
    auto parsed = gfx::ParseMaterialType(*text);
    if (!parsed) {
        std::cerr << parsed.error().Message() << '\n';
        return 1;
    }
    std::cout << "material-type " << parsed->id.String() << "\npasses " << parsed->passes.size() << "\ntextures " << parsed->textures.size() << '\n';
    return 0;
}

int ValidateInstance(const std::filesystem::path& instance_path, const std::filesystem::path& type_path) {
    auto type_text = ReadText(type_path);
    auto instance_text = ReadText(instance_path);
    if (!type_text || !instance_text) {
        std::cerr << (!type_text ? type_text.error().Message() : instance_text.error().Message()) << '\n';
        return 1;
    }
    auto type = gfx::ParseMaterialType(*type_text);
    if (!type) {
        std::cerr << type.error().Message() << '\n';
        return 1;
    }
    auto instance = gfx::ParseMaterialInstance(*instance_text, *type);
    if (!instance) {
        std::cerr << instance.error().Message() << '\n';
        return 1;
    }
    std::cout << "material " << instance->id.String() << "\noverrides " << instance->overrides.size() << '\n';
    return 0;
}
} // namespace

int main(const int argc, const char* const* argv) {
    if (argc == 3 && std::string_view(argv[1]) == "validate-type")
        return ValidateType(argv[2]);
    if (argc == 4 && std::string_view(argv[1]) == "validate-instance")
        return ValidateInstance(argv[2], argv[3]);
    if (argc == 5 && std::string_view(argv[1]) == "cook-type")
        return Cook(true, argv[2], argv[3], argv[4]);
    if (argc == 5 && std::string_view(argv[1]) == "cook-instance")
        return Cook(false, argv[2], argv[3], argv[4]);
    std::cerr << "usage: woki-material validate-type <material-type>\n"
                 "       woki-material validate-instance <material> <material-type>\n"
                 "       woki-material cook-type <material-type> <shader-product> <output>\n"
                 "       woki-material cook-instance <material> <definition-product> <output>\n";
    return 2;
}
