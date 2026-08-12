#include <catch2/catch_test_macros.hpp>

#include <cstdlib>
#include <woki/gfx.hpp>

namespace {
class FakeModule final : public woki::rhi::ShaderModule {
public:
    woki::rhi::Future GetCompilationInfo(woki::rhi::CallbackMode, woki::rhi::ShaderModuleCompilationInfoCallback callback) const override {
        callback(woki::rhi::CompilationInfoRequestStatus::Success, nullptr, {});
        return {.id = 0, .completed = true, .success = true, .message = {}};
    }

    void SetLabel(std::string_view) override {}

    woki::rhi::NativeHandles GetNativeHandles() const noexcept override {
        return {};
    }
};

woki::asset::Product Product(std::string code, std::vector<woki::asset::AssetPath> dependencies = {}) {
    woki::gfx::ShaderPayload payload;
    payload.code = std::move(code);
    payload.interface.entry_points.push_back({.name = "main", .stage = woki::gfx::ShaderStage::Vertex, .inputs = {}, .outputs = {}, .workgroup_size = {1, 1, 1}});
    woki::gfx::NormalizeInterface(payload.interface);
    payload.module_hash = woki::Sha256(payload.code);
    payload.interface_hash = payload.interface.hash;
    payload.variant_hash = woki::Sha256("default");
    payload.dependencies = std::move(dependencies);
    std::ranges::sort(payload.dependencies);
    std::vector<woki::ContentHash> dependency_hashes(payload.dependencies.size(), woki::Sha256("dependency"));
    return *woki::gfx::MakeShaderProduct(payload, woki::Sha256(payload.code), std::move(dependency_hashes));
}
} // namespace

TEST_CASE("Dependency graph replaces reverse edges deterministically") {
    woki::gfx::ShaderDependencyGraph graph;
    const auto shader = woki::gfx::ShaderAssetHandle::Create(2, 1);
    const auto first = *woki::asset::AssetPath::Parse("first.wgsl");
    const auto second = *woki::asset::AssetPath::Parse("second.wgsl");
    const std::array initial{first, second};
    graph.Replace(shader, initial);
    CHECK(graph.Dependents(first) == std::vector{shader});
    const std::array replacement{second};
    graph.Replace(shader, replacement);
    CHECK(graph.Dependents(first).empty());
    CHECK(graph.Dependents(second) == std::vector{shader});
}

TEST_CASE("Reload publication rejects stale and failed candidates without replacing last known good") {
    woki::gfx::ShaderLibrary library([](const woki::rhi::ShaderModuleDesc&) -> woki::Result<woki::ref<woki::rhi::ShaderModule>> { return woki::Ok(woki::ref<woki::rhi::ShaderModule>(new FakeModule)); });
    const auto handle = library.Create();
    REQUIRE(library.Publish(handle, Product("first")));
    const auto original = *library.Borrow(handle);
    auto tampered = Product("tampered");
    tampered.source_hash = woki::Sha256("mutable caller metadata");
    CHECK_FALSE(library.Publish(handle, tampered));
    const auto dependency = *woki::asset::AssetPath::Parse("dependency.wgsl");
    auto inconsistent = Product("inconsistent", {dependency});
    inconsistent.dependency_hashes.clear();
    inconsistent.product_hash = woki::asset::HashProduct(inconsistent);
    CHECK_FALSE(library.Publish(handle, inconsistent));
    woki::gfx::ShaderDependencyGraph graph;
    woki::gfx::ShaderReloadCoordinator coordinator(library, graph, [](auto) -> woki::gfx::ReloadCandidate { std::abort(); }, {});

    auto stale = coordinator.Publish({handle, 0, woki::Ok(Product("stale"))});
    CHECK(stale.outcome == woki::gfx::ReloadOutcome::Stale);
    auto failed = coordinator.Publish({handle, original.Version(), woki::Err(woki::ErrorCode::ParseInvalidFormat, "candidate failed")});
    CHECK(failed.outcome == woki::gfx::ReloadOutcome::Rejected);
    const auto current = *library.Borrow(handle);
    CHECK(current.Version() == original.Version());
    CHECK(&current.Module() == &original.Module());
}

TEST_CASE("Reload publication retains borrowed generations and atomically replaces dependencies") {
    woki::gfx::ShaderLibrary library([](const woki::rhi::ShaderModuleDesc&) -> woki::Result<woki::ref<woki::rhi::ShaderModule>> { return woki::Ok(woki::ref<woki::rhi::ShaderModule>(new FakeModule)); });
    const auto handle = library.Create();
    const auto descriptor = *woki::asset::AssetPath::Parse("shader.woki-shader");
    const auto old_include = *woki::asset::AssetPath::Parse("old.wgsl");
    const auto new_include = *woki::asset::AssetPath::Parse("new.wgsl");
    REQUIRE(library.Publish(handle, Product("first", {descriptor, old_include})));
    const auto borrowed = *library.Borrow(handle);
    woki::gfx::ShaderDependencyGraph graph;
    const std::array old_dependencies{descriptor, old_include};
    graph.Replace(handle, old_dependencies);
    woki::gfx::ShaderReloadCoordinator coordinator(library, graph, [](auto) -> woki::gfx::ReloadCandidate { std::abort(); }, {});

    const auto event = coordinator.Publish({handle, borrowed.Version(), woki::Ok(Product("second", {descriptor, new_include}))});
    REQUIRE(event.outcome == woki::gfx::ReloadOutcome::Published);
    CHECK(graph.Dependents(old_include).empty());
    CHECK(graph.Dependents(new_include) == std::vector{handle});
    CHECK(graph.Dependents(descriptor) == std::vector{handle});
    CHECK(borrowed.generation->payload.code == "first");
    CHECK(&library.Borrow(handle)->Module() != &borrowed.Module());
}

TEST_CASE("Borrowed shader generations outlive their library") {
    std::optional<woki::gfx::BorrowedShader> borrowed;
    {
        woki::gfx::ShaderLibrary library([](const woki::rhi::ShaderModuleDesc&) -> woki::Result<woki::ref<woki::rhi::ShaderModule>> { return woki::Ok(woki::ref<woki::rhi::ShaderModule>(new FakeModule)); });
        const auto handle = library.Create();
        REQUIRE(library.Publish(handle, Product("retained")));
        borrowed = *library.Borrow(handle);
    }
    REQUIRE(borrowed);
    CHECK(borrowed->generation->payload.code == "retained");
    CHECK(borrowed->Version() == 1);
}
