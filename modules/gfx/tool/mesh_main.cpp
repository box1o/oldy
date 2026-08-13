#include <algorithm>
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
        return Err(ErrorCode::FileNotFound, "unable to open mesh descriptor");
    std::ostringstream output;
    output << stream.rdbuf();
    return Ok(output.str());
}

Result<std::vector<std::byte>> ReadBinary(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary | std::ios::ate);
    if (!stream)
        return Err(ErrorCode::FileNotFound, "unable to open mesh product");
    const auto size = stream.tellg();
    if (size < 0)
        return Err(ErrorCode::FileReadError, "unable to size mesh product");
    std::vector<std::byte> bytes(static_cast<size_t>(size));
    stream.seekg(0);
    stream.read(reinterpret_cast<char*>(bytes.data()), size);
    if (!stream)
        return Err(ErrorCode::FileReadError, "unable to read mesh product");
    return Ok(std::move(bytes));
}

void Print(const gfx::MeshProduct& mesh) {
    u64 keys{};
    for (const auto& clip : mesh.animations)
        for (const auto& channel : clip.channels)
            keys += channel.times.size();
    const auto& finest = mesh.lods.back();
    std::cout << "mesh\nsubmeshes " << finest.submeshes.size() << "\nlods " << mesh.lods.size() << "\nvertices " << finest.vertex_count << "\nindices " << finest.index_count << "\nbones " << mesh.skeleton.size()
              << "\nclips " << mesh.animations.size() << "\nkeys " << keys << "\nimporter " << mesh.importer << '\n';
    for (const auto& clip : mesh.animations)
        std::cout << "animation " << clip.name << " duration=" << clip.duration << " channels=" << clip.channels.size() << '\n';
}

Result<asset::BuildOutput> Build(const std::filesystem::path& descriptor_path, const std::optional<std::filesystem::path>& shader_path = std::nullopt) {
    std::string descriptor;
    TRY_ASSIGN(descriptor, ReadText(descriptor_path));
    auto parsed_source = gfx::ParseMeshSource(descriptor);
    if (!parsed_source)
        return Err(std::move(parsed_source).error());
    gfx::MeshSource source = std::move(*parsed_source);
    auto root = descriptor_path.parent_path().parent_path();
    ref<asset::DirectoryMount> mount;
    TRY_ASSIGN(mount, asset::DirectoryMount::Create(root));
    auto vfs = createRef<asset::Vfs>();
    TRY_VOID(vfs->MountAt("assets", asset::AssetScheme::Engine, {}, 0, mount));
    gfx::ModelImporterRegistry registry_value;
    TRY_ASSIGN(registry_value, gfx::CreateDefaultModelImporterRegistry());
    auto registry = createRef<const gfx::ModelImporterRegistry>(std::move(registry_value));
    std::optional<asset::Product> material_shader;
    if (shader_path) {
        std::vector<std::byte> bytes;
        TRY_ASSIGN(bytes, ReadBinary(*shader_path));
        TRY_ASSIGN(material_shader, asset::ParseProduct(bytes));
    }
    gfx::MeshBuilder::ResolveProduct resolve;
    if (material_shader)
        resolve = [shader = std::move(*material_shader)](asset::AssetId) -> Result<asset::Product> { return Ok(shader); };
    gfx::MeshBuilder builder(vfs, registry, std::move(resolve));
    asset::BuildContext context;
    asset::Product product;
    TRY_ASSIGN(product, builder.Build({source.asset_id, asset::AssetUri::Engine(*asset::AssetPath::Parse(descriptor_path.filename().string())), Sha256(descriptor), {}, 1}, context, std::as_bytes(std::span(descriptor))));
    return Ok(asset::BuildOutput{std::move(product), context.GeneratedProducts(), context.SourceDependencies(), context.ProductDependencies()});
}

int Failure(const Error& error) {
    std::cerr << error.Message() << '\n';
    return 1;
}

Result<void> WriteProduct(const asset::Product& product, const std::filesystem::path& path) {
    auto bytes = asset::SerializeProduct(product, {.max_payload_bytes = 1024U * 1024U * 1024U});
    if (!bytes)
        return Err(std::move(bytes).error());
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output.write(reinterpret_cast<const char*>(bytes->data()), static_cast<std::streamsize>(bytes->size()));
    return output ? Ok() : Err(ErrorCode::FileWriteError, "unable to write mesh product");
}
} // namespace

int main(const int argc, const char* const* argv) {
    if (argc == 3 && std::string_view(argv[1]) == "validate") {
        auto text = ReadText(argv[2]);
        if (!text)
            return Failure(text.error());
        auto source = gfx::ParseMeshSource(*text);
        if (!source)
            return Failure(source.error());
        std::cout << "mesh-import " << source->asset_id.String() << "\nsource " << source->source_uri.String() << '\n';
        return 0;
    }
    if (argc == 4 && std::string_view(argv[1]) == "build") {
        auto output = Build(argv[2]);
        auto* product = output ? &output->product : nullptr;
        if (!output)
            return Failure(output.error());
        auto bytes = asset::SerializeProduct(*product, {.max_payload_bytes = 1024U * 1024U * 1024U});
        if (!bytes)
            return Failure(bytes.error());
        std::ofstream stream(argv[3], std::ios::binary);
        stream.write(reinterpret_cast<const char*>(bytes->data()), static_cast<std::streamsize>(bytes->size()));
        if (!stream)
            return Failure(MakeError(ErrorCode::FileWriteError, "unable to write mesh product"));
        auto mesh = gfx::ParseMeshProduct(product->payload);
        if (!mesh)
            return Failure(mesh.error());
        Print(*mesh);
        return 0;
    }
    if (argc == 5 && std::string_view(argv[1]) == "cook") {
        auto output = Build(argv[2], std::filesystem::path(argv[4]));
        if (!output)
            return Failure(output.error());
        auto& product = output->product;
        const std::filesystem::path root = argv[3];
        const std::filesystem::path directory = root / "mesh";
        std::error_code error;
        std::filesystem::create_directories(directory, error);
        if (error)
            return Failure(MakeError(ErrorCode::FileWriteError, "unable to create cooked mesh directory"));
        const std::string filename = product.asset_id.String() + ".woki-product";
        if (auto written = WriteProduct(product, directory / filename); !written)
            return Failure(written.error());
        std::vector<std::filesystem::path> product_paths{directory / filename};
        for (const auto& generated : output->generated_products) {
            const char* kind = generated.type == gfx::kTextureProductType ? "textures" : "materials";
            const auto generated_directory = root / kind;
            std::filesystem::create_directories(generated_directory, error);
            if (error)
                return Failure(MakeError(ErrorCode::FileWriteError, "unable to create generated product directory"));
            if (auto written = WriteProduct(generated, generated_directory / (generated.asset_id.String() + ".woki-product")); !written)
                return Failure(written.error());
            product_paths.push_back(generated_directory / (generated.asset_id.String() + ".woki-product"));
        }
        std::ranges::sort(product_paths);
        std::ofstream product_list(directory / "products.list", std::ios::trunc);
        for (const auto& path : product_paths)
            product_list << path.generic_string() << '\n';
        if (!product_list)
            return Failure(MakeError(ErrorCode::FileWriteError, "unable to write generated product list"));
        asset::AssetManifest manifest;
        auto locator = asset::AssetUri::Parse("engine://cooked/mesh/" + filename);
        if (!locator)
            return Failure(locator.error());
        std::vector<asset::ProductChunkSemantic> chunks;
        for (const auto& chunk : product.chunks)
            chunks.push_back(chunk.semantic);
        if (auto added = manifest.Add({product.asset_id, product.type, product.product_hash, product.schema_version, "native", product.target_fingerprint, {*locator, 0, 0}, std::move(chunks)}); !added)
            return Failure(added.error());
        auto manifest_bytes = manifest.Serialize();
        if (!manifest_bytes)
            return Failure(manifest_bytes.error());
        std::ofstream manifest_stream(directory / "manifest.wkam", std::ios::binary | std::ios::trunc);
        manifest_stream.write(reinterpret_cast<const char*>(manifest_bytes->data()), static_cast<std::streamsize>(manifest_bytes->size()));
        if (!manifest_stream)
            return Failure(MakeError(ErrorCode::FileWriteError, "unable to write cooked mesh manifest"));
        return 0;
    }
    if (argc == 3 && (std::string_view(argv[1]) == "inspect" || std::string_view(argv[1]) == "deps")) {
        auto bytes = ReadBinary(argv[2]);
        if (!bytes)
            return Failure(bytes.error());
        auto product = asset::ParseProduct(*bytes, {.max_payload_bytes = 1024U * 1024U * 1024U});
        if (!product)
            return Failure(product.error());
        if (std::string_view(argv[1]) == "deps") {
            for (const auto& dependency : product->dependencies)
                std::cout << dependency.asset_id.String() << ' ' << dependency.product_hash.Hex() << '\n';
            return 0;
        }
        auto mesh = gfx::ParseMeshProduct(product->payload);
        if (!mesh)
            return Failure(mesh.error());
        Print(*mesh);
        return 0;
    }
    std::cerr << "usage: woki-mesh validate <descriptor>\n       woki-mesh build <descriptor> <product>\n       woki-mesh cook <descriptor> <cooked-root> <material-shader-product>\n       woki-mesh inspect <product>\n  "
                 "     woki-mesh deps <product>\n";
    return 2;
}
