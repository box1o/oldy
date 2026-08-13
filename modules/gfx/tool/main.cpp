#include <fstream>
#include <iostream>
#include <algorithm>
#include <filesystem>
#include <iomanip>
#include <cctype>

#include <woki/gfx/advanced.hpp>
#include <woki/gfx/advanced/product.hpp>
#include <woki/gfx/advanced/variant.hpp>

namespace {
using namespace woki;

Result<std::vector<std::byte>> ReadBinary(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary | std::ios::ate);
    if (!stream)
        return Err(ErrorCode::FileNotFound, "unable to open input file");
    const auto end = stream.tellg();
    if (end < 0)
        return Err(ErrorCode::FileReadError, "unable to determine input size");
    std::vector<std::byte> bytes(static_cast<std::size_t>(end));
    stream.seekg(0);
    if (!bytes.empty() && !stream.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size())))
        return Err(ErrorCode::FileReadError, "unable to read input file");
    return Ok(std::move(bytes));
}

bool Print(const std::vector<gfx::ShaderDiagnostic>& diagnostics) {
    for (const auto& diagnostic : diagnostics) {
        std::cerr << diagnostic.code << ": " << diagnostic.message;
        if (diagnostic.range.path)
            std::cerr << " [" << diagnostic.range.path->String() << ':' << diagnostic.range.byte_offset << ']';
        std::cerr << '\n';
    }
    return std::ranges::none_of(diagnostics, [](const auto& value) { return value.severity == gfx::DiagnosticSeverity::Error; });
}

struct Input {
    asset::Vfs vfs;
    gfx::ShaderDescriptor descriptor;
    gfx::ComposedSource source;
};

Result<Input> Load(const std::filesystem::path& root, const std::string_view descriptor_name) {
    auto mount = asset::DirectoryMount::Create(root);
    if (!mount)
        return Err(std::move(mount).error());
    Input input;
    input.vfs.AddMount(*mount);
    auto path = asset::AssetPath::Parse(descriptor_name);
    if (!path)
        return Err(std::move(path).error());
    auto text = input.vfs.ReadText(*path);
    if (!text)
        return Err(std::move(text).error());
    auto parsed = gfx::ParseShaderDescriptor(*path, *text);
    if (!Print(parsed.diagnostics))
        return Err(ErrorCode::ParseInvalidFormat, "shader descriptor is invalid");
    input.descriptor = std::move(parsed.descriptor);
    input.source = gfx::ShaderSourceResolver(input.vfs).Compose(input.descriptor, *path);
    if (!Print(input.source.diagnostics))
        return Err(ErrorCode::ParseInvalidFormat, "shader source composition failed");
    return Ok(std::move(input));
}

void Reflect(const gfx::ShaderInterface& interface) {
    std::cout << "interface " << interface.hash.Hex() << '\n';
    for (const auto& entry : interface.entry_points)
        std::cout << "entry " << static_cast<u32>(entry.stage) << ' ' << entry.name << ' ' << entry.semantic << '\n';
    for (const auto& binding : interface.bindings)
        std::cout << "binding " << binding.group << ' ' << binding.binding << ' ' << static_cast<u32>(binding.kind) << ' ' << static_cast<u32>(binding.stages) << ' ' << binding.semantic << ' ' << binding.group_semantic
                  << '\n';
    for (const auto& value : interface.overrides)
        std::cout << "override " << value.id << ' ' << value.name << '\n';
}

std::string Identifier(std::string value) {
    for (char& character : value)
        if (!std::isalnum(static_cast<unsigned char>(character)) && character != '_')
            character = '_';
    if (value.empty() || std::isdigit(static_cast<unsigned char>(value.front())))
        value.insert(value.begin(), '_');
    return value;
}

Result<void> GenerateInterfaceHeader(const gfx::ShaderDescriptor& descriptor, const gfx::ShaderInterface& interface, const std::filesystem::path& output) {
    if (!output.parent_path().empty())
        std::filesystem::create_directories(output.parent_path());
    std::ofstream stream(output, std::ios::binary | std::ios::trunc);
    if (!stream)
        return Err(ErrorCode::FileWriteError, "unable to create generated shader interface header");
    stream << "#pragma once\n#include <array>\n#include <string_view>\n#include <woki/gfx/advanced/reflection.hpp>\n\nnamespace woki::gfx::generated::" << Identifier(descriptor.name) << " {\n";
    stream << "inline constexpr ContentHash kInterfaceHash{std::array<u8, ContentHash::kSize>{";
    for (std::size_t index = 0; index < interface.hash.Bytes().size(); ++index) {
        if (index != 0)
            stream << ',';
        stream << static_cast<u32>(interface.hash.Bytes()[index]);
    }
    stream << "}};\n";
    for (const auto& binding : interface.bindings) {
        const auto name = Identifier(binding.semantic.empty() ? "Binding_" + std::to_string(binding.group) + "_" + std::to_string(binding.binding) : binding.semantic);
        stream << "inline constexpr ShaderBindingId k" << name << "{" << binding.group << "U," << binding.binding << "U};\n";
    }
    for (const auto& entry : interface.entry_points) {
        const auto semantic = Identifier(entry.semantic.empty() ? entry.name : entry.semantic);
        stream << "inline constexpr std::string_view k" << semantic << "Entry = \"" << entry.name << "\";\n";
        stream << "inline constexpr std::string_view k" << semantic << "Semantic = \"" << entry.semantic << "\";\n";
    }
    stream << "} // namespace woki::gfx::generated::" << Identifier(descriptor.name) << "\n";
    return stream ? Ok() : Result<void>(Err(ErrorCode::FileWriteError, "unable to write generated shader interface header"));
}

Result<gfx::ShaderPayload> ReadShaderPayload(const std::filesystem::path& path) {
    auto bytes = ReadBinary(path);
    if (!bytes)
        return Err(std::move(bytes).error());
    auto product = asset::ParseProduct(*bytes);
    if (!product || product->type != gfx::kShaderProductType)
        return Err(ErrorCode::ParseInvalidFormat, "not a shader product");
    return gfx::ParseShaderPayload(product->payload);
}

int SourceCommand(const std::string_view command, const std::filesystem::path& root, const std::string_view descriptor_name, const std::filesystem::path& output) {
    auto input = Load(root, descriptor_name);
    if (!input) {
        std::cerr << input.error().Message() << '\n';
        return 1;
    }
    if (command == "deps") {
        for (const auto& dependency : input->source.dependencies)
            std::cout << dependency.String() << '\n';
        return 0;
    }
    if (command == "compose") {
        std::cout << input->source.code;
        return 0;
    }
    if (command == "variants") {
        const auto plan = gfx::PlanVariants(input->descriptor);
        if (!Print(plan.diagnostics))
            return 1;
        for (const auto& variant : plan.variants)
            std::cout << variant.hash.Hex() << '\n';
        return 0;
    }
    auto compiler = gfx::CreateTintShaderCompiler();
    auto compiled = compiler->Compile({input->descriptor, input->source});
    if (!Print(compiled.diagnostics) || !compiled.validated_with_tint)
        return 1;
    if (command == "validate") {
        std::cout << "valid (tint)\n";
        return 0;
    }
    if (command == "reflect" || command == "interface") {
        Reflect(compiled.interface);
        return 0;
    }
    if (command == "bindings") {
        for (const auto& binding : compiled.interface.bindings)
            std::cout << binding.group << ':' << binding.binding << ' ' << binding.semantic << ' ' << binding.group_semantic << ' ' << static_cast<u32>(binding.kind) << '\n';
        return 0;
    }
    if (command == "generate-interface") {
        auto generated = GenerateInterfaceHeader(input->descriptor, compiled.interface, output);
        if (!generated) {
            std::cerr << generated.error().Message() << '\n';
            return 1;
        }
        return 0;
    }
    if (command != "compile" || output.empty())
        return 2;
    const auto variants = gfx::PlanVariants(input->descriptor);
    if (!variants.diagnostics.empty()) {
        Print(variants.diagnostics);
        return 1;
    }
    if (variants.variants.size() != 1) {
        std::cerr << "compile requires an explicit single variant; use variants to enumerate override-backed identities\n";
        return 2;
    }
    gfx::ShaderPayload payload{.code = std::move(compiled.code),
        .interface = std::move(compiled.interface),
        .dependencies = input->source.dependencies,
        .source_map = input->descriptor.compile_options.emit_source_map ? input->source.source_map : std::vector<gfx::SourceMapEntry>{},
        .module_hash = {},
        .interface_hash = {},
        .variant_hash = {}};
    payload.module_hash = Sha256(payload.code);
    payload.interface_hash = payload.interface.hash;
    payload.variant_hash = variants.variants.front().hash;
    auto product = gfx::MakeShaderProduct(
        asset::AssetId::FromName("engine://" + std::string(descriptor_name)),
        payload,
        Sha256(input->source.code),
        {}
    );
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
    return stream ? 0 : 1;
}

int Inspect(const std::filesystem::path& path) {
    auto bytes = ReadBinary(path);
    if (!bytes) {
        std::cerr << bytes.error().Message() << '\n';
        return 1;
    }
    auto product = asset::ParseProduct(*bytes);
    if (!product || product->type != gfx::kShaderProductType) {
        std::cerr << "not a shader product\n";
        return 1;
    }
    auto payload = gfx::ParseShaderPayload(product->payload);
    if (!payload) {
        std::cerr << payload.error().Message() << '\n';
        return 1;
    }
    std::cout << "asset " << product->asset_id.String() << '\n'
              << "product " << product->product_hash.Hex() << '\n'
              << "module " << payload->module_hash.Hex() << '\n'
              << "variant " << payload->variant_hash.Hex() << '\n';
    Reflect(payload->interface);
    return 0;
}

int DiffInterface(const std::filesystem::path& expected_path, const std::filesystem::path& actual_path) {
    auto expected = ReadShaderPayload(expected_path);
    auto actual = ReadShaderPayload(actual_path);
    if (!expected || !actual) {
        std::cerr << (!expected ? expected.error().Message() : actual.error().Message()) << '\n';
        return 1;
    }
    const auto diff = gfx::DiffInterfaces(expected->interface, actual->interface);
    std::cout << "expected " << diff.expected_hash.Hex() << "\nactual " << diff.actual_hash.Hex() << '\n';
    for (const auto& item : diff.differences)
        std::cout << static_cast<u32>(item.kind) << ' ' << item.path << " expected=\"" << item.expected << "\" actual=\"" << item.actual << "\"\n";
    return diff.Compatible() ? 0 : 1;
}
} // namespace

int main(const int argc, const char* const* argv) {
    if (argc < 3) {
        std::cerr << "usage: woki-shader <validate|compile|reflect|interface|bindings|variants|deps|compose> <descriptor> [root]\n"
                     "       woki-shader <compile|generate-interface> <descriptor> <output> [root]\n"
                     "       woki-shader inspect <product>\n       woki-shader diff-interface <expected-product> <actual-product>\n";
        return 2;
    }
    const std::string_view command = argv[1];
    if (command == "inspect")
        return Inspect(argv[2]);
    if (command == "diff-interface")
        return argc >= 4 ? DiffInterface(argv[2], argv[3]) : 2;
    const bool writes_output = command == "compile" || command == "generate-interface";
    const std::filesystem::path output = writes_output && argc >= 4 ? argv[3] : "";
    const std::filesystem::path root = writes_output ? (argc >= 5 ? argv[4] : ".") : (argc >= 4 ? argv[3] : ".");
    return SourceCommand(command, root, argv[2], output);
}
