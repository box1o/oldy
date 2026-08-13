#include <algorithm>
#include <fstream>
#include <iostream>

#include <woki/asset.hpp>

namespace {
using namespace woki;

Result<std::vector<std::byte>> Read(const std::filesystem::path& path) {
    std::error_code error;
    const auto size = std::filesystem::file_size(path, error);
    if (error)
        return Err(ErrorCode::FileNotFound, "input file was not found");
    if (size > 1024ULL * 1024ULL * 1024ULL)
        return Err(ErrorCode::OutOfRange, "input file exceeds tool limit");
    std::ifstream stream(path, std::ios::binary);
    std::vector<std::byte> bytes(static_cast<std::size_t>(size));
    if (!stream || (!bytes.empty() && !stream.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()))))
        return Err(ErrorCode::FileReadError, "unable to read input file");
    return Ok(std::move(bytes));
}

Result<void> WriteAtomic(const std::filesystem::path& path, const std::span<const std::byte> bytes) {
    auto temporary = path;
    temporary += ".tmp";
    {
        std::ofstream stream(temporary, std::ios::binary | std::ios::trunc);
        if (!stream || (!bytes.empty() && !stream.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()))) || !stream.flush())
            return Err(ErrorCode::FileWriteError, "unable to write temporary output");
    }
    std::error_code error;
    std::filesystem::rename(temporary, path, error);
    if (error) {
        std::filesystem::remove(temporary, error);
        return Err(ErrorCode::FileWriteError, "unable to publish output atomically");
    }
    return Ok();
}

Result<asset::Product> Product(const std::filesystem::path& path) {
    auto bytes = Read(path);
    return bytes ? asset::ParseProduct(*bytes, {.max_payload_bytes = 1024U * 1024U * 1024U, .max_decompressed_chunk_bytes = 1024U * 1024U * 1024U}) : Result<asset::Product>(Err(std::move(bytes).error()));
}

int Failure(const Error& error) {
    std::cerr << error.Message() << '\n';
    return 1;
}
} // namespace

int main(int argc, char** argv) {
    using namespace woki;
    if (argc < 3) {
        std::cerr << "usage: woki-asset <validate|build|inspect|deps|upgrade|package> ...\n";
        return 2;
    }
    const std::string_view command = argv[1];
    if (command == "validate" || command == "inspect" || command == "deps") {
        auto product = Product(argv[2]);
        if (!product)
            return Failure(product.error());
        if (command == "inspect")
            std::cout << product->asset_id.String() << " type=" << product->type << " schema=" << product->schema_version << " builder=" << product->builder_version << " container=" << product->container_version
                      << " chunks=" << product->chunks.size() << " hash=" << product->product_hash.Hex() << '\n';
        if (command == "deps")
            for (const auto& dependency : product->dependencies)
                std::cout << dependency.asset_id.String() << ' ' << dependency.product_hash.Hex() << '\n';
        return 0;
    }
    if ((command == "build" || command == "upgrade") && argc == 4) {
        auto bytes = Read(argv[2]);
        if (!bytes)
            return Failure(bytes.error());
        if (bytes->size() >= 6) {
            const u32 version_bits = static_cast<u32>(std::to_integer<u8>((*bytes)[4])) | (static_cast<u32>(std::to_integer<u8>((*bytes)[5])) << 8U);
            const u16 version = static_cast<u16>(version_bits);
            if (asset::ProductRequiresRebuild(version))
                return Failure(MakeError(ErrorCode::ParseInvalidFormat, "product container requires a source rebuild; no lossless migration exists"));
        }
        auto product = asset::ParseProduct(*bytes, {.max_payload_bytes = 1024U * 1024U * 1024U, .max_decompressed_chunk_bytes = 1024U * 1024U * 1024U});
        if (!product)
            return Failure(product.error());
        auto output = asset::SerializeProduct(*product, {.max_payload_bytes = 1024U * 1024U * 1024U, .max_decompressed_chunk_bytes = 1024U * 1024U * 1024U});
        if (!output)
            return Failure(output.error());
        auto written = WriteAtomic(argv[3], *output);
        return written ? 0 : Failure(written.error());
    }
    if (command == "package" && argc >= 9) {
        int separator = 6;
        while (separator < argc && std::string_view(argv[separator]) != "--")
            ++separator;
        if (separator == 6 || separator + 1 >= argc)
            return 2;
        auto uri = asset::AssetUri::Parse(argv[5]);
        if (!uri)
            return Failure(uri.error());
        const bool all_roots = separator == 7 && std::string_view(argv[6]) == "all";
        std::vector<asset::AssetId> roots;
        if (!all_roots)
            for (int i = 6; i < separator; ++i) {
                auto id = asset::AssetId::Parse(argv[i]);
                if (!id)
                    return Failure(id.error());
                roots.push_back(*id);
            }
        std::vector<std::filesystem::path> paths;
        if (separator + 2 == argc && std::filesystem::is_directory(argv[separator + 1])) {
            std::error_code error;
            for (std::filesystem::recursive_directory_iterator it(argv[separator + 1], std::filesystem::directory_options::skip_permission_denied, error), end; !error && it != end; it.increment(error))
                if (it->is_regular_file(error) && it->path().extension() == ".woki-product")
                    paths.push_back(it->path());
            if (error)
                return Failure(MakeError(ErrorCode::FileReadError, "unable to enumerate package product inputs"));
            std::ranges::sort(paths);
        } else
            for (int i = separator + 1; i < argc; ++i) {
                const std::string_view argument = argv[i];
                if (!argument.empty() && argument.front() == '@') {
                    std::ifstream list{std::filesystem::path(argument.substr(1))};
                    std::string path;
                    while (std::getline(list, path))
                        if (!path.empty())
                            paths.emplace_back(path);
                    if (!list.eof())
                        return Failure(MakeError(ErrorCode::FileReadError, "unable to read package response file"));
                } else
                    paths.emplace_back(argument);
            }
        std::vector<asset::Product> products;
        for (const auto& path : paths) {
            auto product = Product(path);
            if (!product)
                return Failure(product.error());
            products.push_back(std::move(*product));
        }
        if (all_roots)
            for (const auto& product : products)
                roots.push_back(product.asset_id);
        auto package = asset::BuildPackage(products, roots,
            {
                .profile = asset::PackageProfile::Shipping,
                .target = argv[4],
                .capability_fingerprint = {},
                .bundle_uri = *uri,
            });
        if (!package)
            return Failure(package.error());
        auto manifest = package->manifest.Serialize();
        if (!manifest)
            return Failure(manifest.error());
        auto bundle_written = WriteAtomic(argv[2], package->bundle);
        if (!bundle_written)
            return Failure(bundle_written.error());
        auto manifest_written = WriteAtomic(argv[3], *manifest);
        return manifest_written ? 0 : Failure(manifest_written.error());
    }
    std::cerr << "usage: woki-asset validate|inspect|deps <product>\n       woki-asset build|upgrade <product> <output>\n       woki-asset package <bundle> <manifest> <target> <bundle-uri> <root>... -- <product>...\n";
    return 2;
}
