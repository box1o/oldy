#include <catch2/catch_test_macros.hpp>

#include <array>

#include <woki/asset.hpp>

TEST_CASE("WKAS products serialize deterministically and round trip") {
    const std::array payload{std::byte{1}, std::byte{2}, std::byte{3}};
    auto product = woki::asset::MakeProduct(0x52444853U, 3, woki::Sha256("source"), {woki::Sha256("include")}, {payload.begin(), payload.end()});

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
    REQUIRE(parsed->dependency_hashes == product.dependency_hashes);
    REQUIRE(parsed->product_hash == product.product_hash);
    REQUIRE(parsed->payload == product.payload);
}

TEST_CASE("WKAS parsing rejects malformed and oversized products") {
    auto product = woki::asset::MakeProduct(1, 1, woki::Sha256("source"), {}, {std::byte{1}, std::byte{2}});
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
