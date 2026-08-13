#include <catch2/catch_test_macros.hpp>

#include <woki/gfx/advanced.hpp>

#include "null_rhi_fixture.hpp"

using namespace woki;

TEST_CASE("Standard feature registry exposes the complete baseline with stable scopes") {
    gfx::FeatureRegistry registry;
    REQUIRE(gfx::RegisterStandardFeatures(registry));
    const std::array names{"mesh", "gpu-visibility", "depth", "hiz-depth-pyramid", "velocity", "lighting", "forward", "transparent", "shadows", "sky", "temporal", "bloom", "exposure", "editor-overlay", "presentation"};
    for (const std::string_view name : names) {
        INFO(name);
        const auto* metadata = registry.Metadata(StringId(name));
        REQUIRE(metadata != nullptr);
        CHECK_FALSE(metadata->compatible_render_paths.empty());
        REQUIRE(registry.Compile(StringId(name), {}));
    }
    CHECK(registry.Metadata(StringId("lighting"))->scope == gfx::FeatureScope::ViewFamily);
    CHECK(registry.Metadata(StringId("shadows"))->scope == gfx::FeatureScope::ViewFamily);
    CHECK(registry.Metadata(StringId("temporal"))->scope == gfx::FeatureScope::View);
    CHECK(registry.KnowsCapability(StringId("hdr")));
    CHECK(registry.KnowsExtensionPoint(StringId("opaque")));
}

TEST_CASE("Null executes compute and offscreen-style graph work as commands, not pixels") {
    gfx::test::NullDevice fixture;
    auto shader = fixture.device->CreateShaderModule({.code = "@compute @workgroup_size(1) fn main() {}", .label = "test-compute"});
    REQUIRE(shader);
    auto pipeline = fixture.device->CreateComputePipeline({.compute = {.module = shader->get(), .entry_point = "main"}, .label = "test-compute"});
    REQUIRE(pipeline);
    gfx::RenderGraphBuilder builder;
    const auto storage = builder.CreateBuffer({.label = "compute-output", .size = 64, .alignment = 4, .usage = rhi::BufferUsage::Storage | rhi::BufferUsage::CopySrc});
    auto compute = builder.AddPass("cluster-and-visibility", gfx::PassKind::Compute);
    const auto output = compute.Write(storage, gfx::GraphAccess::StorageWrite);
    compute.Execute([pipeline = pipeline->get()](gfx::RenderGraphContext& context) -> Result<void> {
        auto* encoder = context.ComputeEncoder();
        if (encoder == nullptr)
            return Err(ErrorCode::InvalidState, "missing compute encoder");
        encoder->SetPipeline(*pipeline);
        encoder->DispatchWorkgroups(4, 2, 1);
        return Ok();
    });
    REQUIRE(builder.Export(output));
    auto graph = builder.Compile(32, 16);
    REQUIRE(graph);
    const auto hash = graph->DeterministicHash();
    gfx::GraphExecutor executor(fixture.device, std::move(*graph));
    auto frame = executor.Begin(32, 16);
    REQUIRE(frame);
    REQUIRE(frame->Execute());
    CHECK(executor.Graph().DeterministicHash() == hash);
    CHECK(gfx::test::LogContains(*fixture.null_device, "compute.begin"));
    CHECK(gfx::test::LogContains(*fixture.null_device, "compute.dispatch 4 2 1"));
    CHECK(gfx::test::LogContains(*fixture.null_device, "queue.submit"));
    CHECK(fixture.diagnostics.empty());
}

TEST_CASE("Null command logs record graph ordering and suppress submission after executor failure") {
    gfx::test::NullDevice fixture;
    gfx::RenderGraphBuilder builder;
    auto first = builder.AddPass("temporal", gfx::PassKind::Compute);
    first.SideEffect("history").Execute([](gfx::RenderGraphContext&) { return Ok(); });
    auto failed = builder.AddPass("readback", gfx::PassKind::Compute);
    failed.DependsOn(first.Handle()).SideEffect("readback").Execute([](gfx::RenderGraphContext&) { return Err(ErrorCode::InvalidState, "injected executor failure"); });
    auto graph = builder.Compile(8, 8);
    REQUIRE(graph);
    gfx::GraphExecutor executor(fixture.device, std::move(*graph));
    auto frame = executor.Begin(8, 8);
    REQUIRE(frame);
    auto result = frame->Execute();
    REQUIRE_FALSE(result);
    CHECK(result.error().Message().find("injected executor failure") != std::string_view::npos);
    CHECK(std::ranges::count(rhi::NullCommandLog(*fixture.null_device), std::string("compute.begin")) == 2);
    CHECK(std::ranges::count(rhi::NullCommandLog(*fixture.null_device), std::string("compute.end")) == 2);
    CHECK_FALSE(gfx::test::LogContains(*fixture.null_device, "queue.submit"));
}
