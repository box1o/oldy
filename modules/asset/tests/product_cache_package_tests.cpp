#include <array>
#include <fstream>

#include <catch2/catch_test_macros.hpp>

#include <woki/asset.hpp>

#include "test_support.hpp"

using namespace woki;
using namespace woki::asset;

TEST_CASE("product v3 validates chunk bounds alignment ordering and checksums") {
    const auto id = AssetId::FromName("chunked");
    auto product = test::ProductFor(id, "abcdefgh");
    product.chunks = {
        {ProductChunkSemantic::Header, ProductCompression::None, 1, 0, 3, 3, Sha256(std::span(product.payload).first(3))},
        {ProductChunkSemantic::Debug, ProductCompression::None, 1, 3, 5, 5, Sha256(std::span(product.payload).subspan(3))},
    };
    product.product_hash = HashProduct(product);
    REQUIRE(ValidateProduct(product));

    auto invalid = product;
    invalid.chunks[1].offset = 2;
    invalid.product_hash = HashProduct(invalid);
    REQUIRE_FALSE(ValidateProduct(invalid));
    invalid = product;
    invalid.chunks[1].offset = 9;
    invalid.product_hash = HashProduct(invalid);
    REQUIRE_FALSE(ValidateProduct(invalid));
    invalid = product;
    invalid.chunks[0].alignment = 3;
    invalid.product_hash = HashProduct(invalid);
    REQUIRE_FALSE(ValidateProduct(invalid));
    invalid = product;
    invalid.chunks[0].checksum = Sha256("wrong");
    invalid.product_hash = HashProduct(invalid);
    REQUIRE_FALSE(ValidateProduct(invalid));
    REQUIRE_FALSE(ValidateProduct(product, {.max_payload_bytes = 7}));
    REQUIRE(ProductRequiresRebuild(1));
    REQUIRE_FALSE(ProductRequiresRebuild(3));
    REQUIRE(ProductRequiresRebuild(4));
}

TEST_CASE("ProductReader opens v3 metadata and bounds partial chunk reads") {
    const auto id = AssetId::FromName("reader");
    const auto product = test::ProductFor(id, "reader-data");
    const auto bytes = SerializeProduct(product);
    const auto path = AssetPath::Parse("products/item.wkas");
    const auto uri = AssetUri::Parse("engine://products/item.wkas");
    REQUIRE(bytes);
    REQUIRE(path);
    REQUIRE(uri);
    const auto mount = createRef<MemoryMount>();
    mount->Put(*path, *bytes);
    Vfs vfs;
    REQUIRE(vfs.MountAt("products", AssetScheme::Engine, {}, 0, mount));
    const auto reader = ProductReader::Open(vfs, *uri);
    REQUIRE(reader);
    REQUIRE(reader->Header().container_version == 3);
    REQUIRE(reader->Header().asset_id == id);
    REQUIRE(reader->Chunks().size() == 1);
    const auto chunk = reader->ReadChunk(0);
    REQUIRE(chunk);
    REQUIRE(*chunk == product.payload);
    REQUIRE_FALSE(reader->ReadChunk(1));
    REQUIRE_FALSE(reader->ReadRange(reader->Header().file_size, 1));

    auto corrupted = *bytes;
    corrupted.back() ^= std::byte{1};
    mount->Put(*path, corrupted);
    const auto corrupt_reader = ProductReader::Open(vfs, *uri);
    REQUIRE(corrupt_reader);
    REQUIRE_FALSE(corrupt_reader->ReadChunk(0));
}

TEST_CASE("ProductCache quarantines corrupt and mismatched entries") {
    test::TempDirectory directory;
    auto cache = ProductCache::Create(directory.Path());
    REQUIRE(cache);
    const auto product = test::ProductFor(AssetId::FromName("cached"), "cache-data");
    REQUIRE(cache->Store(product.product_hash, product));
    REQUIRE(cache->Load(product.product_hash)->payload == product.payload);
    REQUIRE(cache->ReadRange(product.product_hash, 0, 4)->size() == 4);
    REQUIRE_FALSE(cache->ReadRange(product.product_hash, 0, static_cast<std::size_t>(*cache->Size(product.product_hash)) + 1));

    const auto path = cache->Path(product.product_hash);
    {
        std::fstream stream(path, std::ios::binary | std::ios::in | std::ios::out);
        REQUIRE(stream);
        stream.seekp(-1, std::ios::end);
        const char corrupt = '\0';
        stream.write(&corrupt, 1);
    }
    REQUIRE_FALSE(cache->Load(product.product_hash));
    REQUIRE_FALSE(std::filesystem::exists(path));
    bool quarantined = false;
    const auto quarantine = directory.Path() / "quarantine";
    if (std::filesystem::exists(quarantine))
        quarantined = std::filesystem::directory_iterator(quarantine) != std::filesystem::directory_iterator{};
    REQUIRE(quarantined);

    const auto other = test::ProductFor(AssetId::FromName("other-cache-entry"), "valid-but-wrong");
    const auto other_bytes = SerializeProduct(other);
    REQUIRE(other_bytes);
    {
        std::ofstream stream(path, std::ios::binary | std::ios::trunc);
        REQUIRE(stream);
        stream.write(reinterpret_cast<const char*>(other_bytes->data()), static_cast<std::streamsize>(other_bytes->size()));
    }
    REQUIRE_FALSE(cache->Load(product.product_hash));
    REQUIRE_FALSE(std::filesystem::exists(path));
}

TEST_CASE("manifest round trips deterministically and rejects duplicate identity") {
    const auto id = AssetId::FromName("manifest");
    const auto uri = AssetUri::Parse("cache://bundle.wkp");
    REQUIRE(uri);
    ManifestEntry entry{id, 7, Sha256("product"), 2, "desktop", Sha256("caps"), {*uri, 12, 34}, {ProductChunkSemantic::Header, ProductChunkSemantic::TextureMip}};
    AssetManifest manifest;
    REQUIRE(manifest.Add(entry));
    REQUIRE_FALSE(manifest.Add(entry));
    const auto bytes = manifest.Serialize();
    REQUIRE(bytes);
    const auto parsed = AssetManifest::Parse(*bytes);
    REQUIRE(parsed);
    REQUIRE(parsed->Resolve(id));
    REQUIRE(parsed->Resolve(id)->locator.offset == 12);
    REQUIRE(parsed->Entries().size() == 1);
    const auto reserialized = parsed->Serialize();
    REQUIRE(reserialized);
    REQUIRE(*reserialized == *bytes);
    REQUIRE_FALSE(AssetManifest::Parse(*bytes, bytes->size() - 1));
}

TEST_CASE("package contains only exact dependency reachability and honors profiles") {
    const auto dependency_id = AssetId::FromName("package-dependency");
    const auto root_id = AssetId::FromName("package-root");
    const auto unrelated_id = AssetId::FromName("package-unrelated");
    auto dependency = test::ProductFor(dependency_id, "dep");
    auto root = test::ProductFor(root_id, "root", {{dependency_id, dependency.product_hash}});
    auto unrelated = test::ProductFor(unrelated_id, "unused");
    const auto bundle = AssetUri::Parse("cache://package.wkp");
    REQUIRE(bundle);
    PackageOptions options{PackageProfile::Development, "desktop", Sha256("caps"), *bundle};
    const std::array products{root, dependency, unrelated};
    const std::array roots{root_id};
    const auto package = BuildPackage(products, roots, options);
    REQUIRE(package);
    REQUIRE(package->manifest.Entries().size() == 2);
    REQUIRE(package->manifest.Resolve(root_id));
    REQUIRE(package->manifest.Resolve(dependency_id));
    REQUIRE_FALSE(package->manifest.Resolve(unrelated_id));

    auto missing = root;
    missing.dependencies[0].product_hash = Sha256("not-the-dependency");
    missing.product_hash = HashProduct(missing);
    REQUIRE(ValidateProduct(missing));
    REQUIRE_FALSE(BuildPackage(std::array{missing, dependency}, roots, options));
    REQUIRE_FALSE(BuildPackage(std::array{root}, roots, options));
}
