#include <catch2/catch_test_macros.hpp>

#include <woki/gfx/advanced.hpp>

TEST_CASE("Variant planning is bounded and deterministic") {
    woki::gfx::ShaderDescriptor descriptor;
    descriptor.permutations = {{"quality", {woki::i64{0}, woki::i64{1}}}, {"skinned", {false, true}}};
    const auto first = woki::gfx::PlanVariants(descriptor, 4);
    const auto second = woki::gfx::PlanVariants(descriptor, 4);
    REQUIRE(first.variants.size() == 4);
    CHECK(first.variants[3].hash == second.variants[3].hash);
    CHECK(woki::gfx::PlanVariants(descriptor, 3).diagnostics[0].code == "SHD4001");
}

TEST_CASE("Variant keys reject undeclared values") {
    woki::gfx::ShaderDescriptor descriptor;
    descriptor.permutations = {{"quality", {woki::i64{0}, woki::i64{1}}}};
    CHECK(woki::gfx::MakeVariantKey(descriptor, {{"quality", woki::i64{1}}}));
    CHECK_FALSE(woki::gfx::MakeVariantKey(descriptor, {{"quality", woki::i64{2}}}));
}
