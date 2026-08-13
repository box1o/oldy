#include <array>
#include <catch2/catch_test_macros.hpp>

#include <woki/asset.hpp>

TEST_CASE("WKAS products serialize deterministically and round trip") {
    const std::array payload{std::byte{1}, std::byte{2}, std::byte{3}};
    const auto dependency = woki::asset::AssetId::FromName("engine://include");
    auto product = woki::asset::MakeProduct(woki::asset::AssetId::FromName("engine://shader"), 0x52444853U, 3, 7, woki::Sha256("source"), {{dependency, woki::Sha256("include")}}, woki::Sha256("target"),
        {payload.begin(), payload.end()});

    const auto first = woki::asset::SerializeProduct(product);
    const auto second = woki::asset::SerializeProduct(product);
    REQUIRE(first);
    REQUIRE(second);
    REQUIRE(*first == *second);

    const auto parsed = woki::asset::ParseProduct(*first);
    REQUIRE(parsed);
    REQUIRE(parsed->type == product.type);
    REQUIRE(parsed->schema_version == product.schema_version);
    REQUIRE(parsed->source_hash == product.source_hash);
    REQUIRE(parsed->dependencies == product.dependencies);
    REQUIRE(parsed->product_hash == product.product_hash);
    REQUIRE(parsed->payload == product.payload);
}

TEST_CASE("WKAS parsing rejects malformed and oversized products") {
    auto product = woki::asset::MakeProduct(woki::asset::AssetId::FromName("engine://test"), 1, 1, 1, woki::Sha256("source"), {}, woki::Sha256("target"), {std::byte{1}, std::byte{2}});
    const auto serialized = woki::asset::SerializeProduct(product);
    REQUIRE(serialized);

    auto bad_magic = *serialized;
    bad_magic[0] = std::byte{'X'};
    REQUIRE_FALSE(woki::asset::ParseProduct(bad_magic));

    auto truncated = *serialized;
    truncated.pop_back();
    REQUIRE_FALSE(woki::asset::ParseProduct(truncated));

    auto corrupted = *serialized;
    corrupted.back() ^= std::byte{1};
    REQUIRE_FALSE(woki::asset::ParseProduct(corrupted));

    product.source_hash = woki::Sha256("mutated metadata");
    REQUIRE_FALSE(woki::asset::ValidateProduct(product));

    REQUIRE_FALSE(woki::asset::ParseProduct(*serialized, {.max_payload_bytes = 1, .max_dependencies = 8}));
}
