#include <catch2/catch_test_macros.hpp>

#include <woki/gfx/advanced.hpp>

namespace {

class FakeBindGroupLayout final : public woki::rhi::BindGroupLayout {
public:
    void SetLabel(std::string_view) override {}

    [[nodiscard]] woki::rhi::NativeHandles GetNativeHandles() const noexcept override {
        return {};
    }
};

class FakePipelineLayout final : public woki::rhi::PipelineLayout {
public:
    void SetLabel(std::string_view) override {}

    [[nodiscard]] woki::rhi::NativeHandles GetNativeHandles() const noexcept override {
        return {};
    }
};

} // namespace

TEST_CASE("Layout keys normalize binding order and stage visibility") {
    woki::gfx::ShaderInterface first;
    first.bindings = {{.group = 2, .binding = 3, .stages = 1, .kind = woki::gfx::ResourceKind::UniformBuffer}, {.group = 0, .binding = 1, .stages = 2, .kind = woki::gfx::ResourceKind::Sampler}};
    auto second = first;
    std::ranges::reverse(second.bindings);
    const auto first_key = woki::gfx::MakePipelineLayoutKey(first);
    const auto second_key = woki::gfx::MakePipelineLayoutKey(second);
    REQUIRE(first_key);
    REQUIRE(second_key);
    CHECK(first_key->hash == second_key->hash);
    CHECK(first_key->groups.size() == 3);
}

TEST_CASE("Layout policy rejects groups beyond object scope") {
    woki::gfx::ShaderInterface interface;
    interface.bindings.push_back({.group = 4});
    CHECK_FALSE(woki::gfx::MakePipelineLayoutKey(interface));
}

TEST_CASE("Layout keys merge split-stage duplicate bindings before grouping") {
    woki::gfx::ShaderInterface split;
    split.bindings = {{.group = 1, .binding = 2, .stages = 1, .kind = woki::gfx::ResourceKind::UniformBuffer}, {.group = 1, .binding = 2, .stages = 2, .kind = woki::gfx::ResourceKind::UniformBuffer}};
    woki::gfx::ShaderInterface merged;
    merged.bindings = {{.group = 1, .binding = 2, .stages = 3, .kind = woki::gfx::ResourceKind::UniformBuffer}};

    const auto split_key = woki::gfx::MakePipelineLayoutKey(split);
    const auto merged_key = woki::gfx::MakePipelineLayoutKey(merged);
    REQUIRE(split_key);
    REQUIRE(merged_key);
    CHECK(*split_key == *merged_key);
    REQUIRE(split_key->groups[1].bindings.size() == 1);
    CHECK(split_key->groups[1].bindings[0].stages == 3);
}

TEST_CASE("Layout keys reject conflicting duplicate bindings") {
    woki::gfx::ShaderInterface interface;
    interface.bindings = {{.group = 1, .binding = 2, .stages = 1, .kind = woki::gfx::ResourceKind::UniformBuffer}, {.group = 1, .binding = 2, .stages = 2, .kind = woki::gfx::ResourceKind::Sampler}};
    CHECK_FALSE(woki::gfx::MakePipelineLayoutKey(interface));
}

TEST_CASE("Dynamic buffer policy contributes deterministically to layout keys") {
    woki::gfx::ShaderInterface interface;
    interface.bindings = {{.group = 0, .binding = 4, .kind = woki::gfx::ResourceKind::UniformBuffer}, {.group = 0, .binding = 7, .kind = woki::gfx::ResourceKind::Sampler}};
    const std::vector<woki::gfx::DynamicBufferBindingPolicy> repeated{{.group = 0, .binding = 4}, {.group = 0, .binding = 4}};
    const auto plain = woki::gfx::MakePipelineLayoutKey(interface);
    const auto dynamic = woki::gfx::MakePipelineLayoutKey(interface, repeated);
    REQUIRE(plain);
    REQUIRE(dynamic);
    CHECK(plain->hash != dynamic->hash);
    CHECK(dynamic->groups[0].dynamic_buffer_bindings == std::vector<woki::u32>{4});

    const woki::gfx::DynamicBufferBindingPolicy invalid{.group = 0, .binding = 7};
    CHECK_FALSE(woki::gfx::MakePipelineLayoutKey(interface, std::span(&invalid, 1)));
}

TEST_CASE("Borrowed layouts retain pipeline and ordered bind-group access") {
    auto generation = woki::createRef<woki::gfx::LayoutGeneration>();
    generation->groups.push_back(woki::createScope<FakeBindGroupLayout>());
    generation->groups.push_back(woki::createScope<FakeBindGroupLayout>());
    generation->ordered_groups = {generation->groups[0].get(), generation->groups[1].get()};
    generation->pipeline = woki::createScope<FakePipelineLayout>();
    const auto* pipeline = generation->pipeline.get();
    const auto* first_group = generation->groups[0].get();
    woki::gfx::BorrowedLayout borrowed{generation};
    generation.reset();

    CHECK(&borrowed.Pipeline() == pipeline);
    REQUIRE(borrowed.BindGroupLayouts().size() == 2);
    CHECK(borrowed.BindGroupLayouts()[0] == first_group);
}
