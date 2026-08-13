#include <fstream>
#include <iostream>
#include <filesystem>

#include <woki/gfx/advanced.hpp>

namespace {
using namespace woki;

bool Diagnostics(const std::vector<gfx::PipelineDiagnostic>& diagnostics) {
    for (const auto& item : diagnostics)
        std::cerr << item.code << ": " << item.message << " [" << item.asset.String() << item.pointer << "]\n";
    return diagnostics.empty();
}

Result<std::vector<std::byte>> Read(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary | std::ios::ate);
    if (!stream)
        return Err(ErrorCode::FileNotFound, "unable to open product");
    const auto size = stream.tellg();
    if (size < 0)
        return Err(ErrorCode::FileReadError, "unable to determine product size");
    std::vector<std::byte> bytes(static_cast<std::size_t>(size));
    stream.seekg(0);
    if (!bytes.empty() && !stream.read(reinterpret_cast<char*>(bytes.data()), size))
        return Err(ErrorCode::FileReadError, "unable to read product");
    return Ok(std::move(bytes));
}

void Print(const gfx::RenderPipelineIR& pipeline) {
    std::cout << pipeline.debug_name << " (" << pipeline.asset_id.String() << ")\nrender path: " << pipeline.render_path_name << "\norder:\n";
    for (const auto& feature : pipeline.features)
        std::cout << "  " << feature.debug_name << " " << feature.factory_version.major << '.' << feature.factory_version.minor << '.' << feature.factory_version.patch << '\n';
    std::cout << "fallbacks:\n";
    for (const auto& fallback : pipeline.fallbacks) {
        std::cout << "  " << fallback.path.String() << " when missing";
        for (const auto& name : fallback.capability_names)
            std::cout << ' ' << name;
        std::cout << '\n';
    }
}

int Main(std::string_view command, const char* input, const char* output, const char* root) {
    gfx::FeatureRegistry registry;
    if (auto standard = gfx::RegisterStandardFeatures(registry); !standard) {
        std::cerr << standard.error().Message() << '\n';
        return 1;
    }
    if (command == "features") {
        for (const auto& feature : registry.Features())
            std::cout << feature.debug_name << ' ' << feature.version.major << '.' << feature.version.minor << '.' << feature.version.patch << '\n';
        return 0;
    }
    if (command == "inspect") {
        auto bytes = Read(input);
        if (!bytes) {
            std::cerr << bytes.error().Message() << '\n';
            return 1;
        }
        auto product = asset::ParseProduct(*bytes);
        if (!product || product->type != gfx::kPipelineProductType) {
            std::cerr << "not a render pipeline product\n";
            return 1;
        }
        auto pipeline = gfx::ParsePipelineProduct(product->payload);
        if (!pipeline) {
            std::cerr << pipeline.error().Message() << '\n';
            return 1;
        }
        std::cout << "asset " << product->asset_id.String() << '\n' << "product " << product->product_hash.Hex() << '\n';
        Print(*pipeline);
        return 0;
    }
    auto mount = asset::DirectoryMount::Create(root);
    if (!mount) {
        std::cerr << mount.error().Message() << '\n';
        return 1;
    }
    asset::Vfs vfs;
    vfs.AddMount(*mount);
    auto path = asset::AssetPath::Parse(input);
    if (!path) {
        std::cerr << path.error().Message() << '\n';
        return 1;
    }
    auto compiled = gfx::RenderPipelineCompiler(vfs, registry).Compile(*path);
    if (!Diagnostics(compiled.diagnostics) || !compiled.pipeline)
        return 1;
    if (command == "validate") {
        std::cout << "valid " << compiled.pipeline->content_hash.Hex() << '\n';
        return 0;
    }
    if (command == "deps") {
        for (const auto& dependency : compiled.pipeline->dependencies)
            std::cout << dependency.path.String() << ' ' << dependency.hash.Hex() << '\n';
        return 0;
    }
    if (command == "explain") {
        Print(*compiled.pipeline);
        for (const auto& extension : compiled.pipeline->extensions) {
            std::cout << extension.debug_name << ':';
            for (const auto& feature : extension.features)
                std::cout << ' ' << feature.debug_name << (feature.instance ? "@" + *feature.instance : "");
            std::cout << '\n';
        }
        return 0;
    }
    if (command == "select") {
        std::vector<StringId> capabilities;
        if (output) {
            std::string list(output);
            std::size_t begin{};
            while (begin <= list.size()) {
                const auto end = list.find(',', begin);
                capabilities.emplace_back(list.substr(begin, end - begin));
                if (end == std::string::npos)
                    break;
                begin = end + 1;
            }
        }
        auto selected = gfx::SelectSupportedPipeline(vfs, registry, *path, std::move(capabilities));
        if (!selected) {
            std::cerr << selected.error().Message() << '\n';
            return 1;
        }
        std::cout << selected->String() << '\n';
        return 0;
    }
    if (command != "compile" || !output)
        return 2;
    auto product = gfx::MakePipelineProduct(*compiled.pipeline);
    if (!product) {
        std::cerr << product.error().Message() << '\n';
        return 1;
    }
    auto bytes = asset::SerializeProduct(*product);
    if (!bytes) {
        std::cerr << bytes.error().Message() << '\n';
        return 1;
    }
    std::ofstream stream(output, std::ios::binary | std::ios::trunc);
    stream.write(reinterpret_cast<const char*>(bytes->data()), static_cast<std::streamsize>(bytes->size()));
    stream.close();
    return stream ? 0 : 1;
}
} // namespace

int main(int argc, const char* const* argv) {
    if (argc < 2) {
        std::cerr << "usage: woki-pipeline <validate|compile|inspect|explain|deps|features|select> [pipeline] [output|capabilities] [root]\n";
        return 2;
    }
    const std::string_view command = argv[1];
    if (command == "features")
        return Main(command, "", nullptr, ".");
    if (argc < 3)
        return 2;
    const char* output = argc > 3 ? argv[3] : nullptr;
    const char* root = argc > 4 ? argv[4] : ".";
    if (command != "compile" && command != "select" && argc > 3)
        root = argv[3];
    return Main(command, argv[2], output, root);
}
