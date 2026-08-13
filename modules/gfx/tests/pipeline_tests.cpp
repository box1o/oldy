#include <filesystem>
#include <catch2/catch_test_macros.hpp>

#include <woki/gfx/advanced.hpp>

namespace {
using namespace woki;

asset::AssetPath Path(std::string_view value) {
    return *asset::AssetPath::Parse(value);
}

struct Fixture {
    Fixture() {
        REQUIRE(gfx::RegisterStandardFeatures(registry));
        vfs.AddMount(mount);
    }

    void Put(std::string_view path, std::string_view text) {
        mount->PutText(Path(path), text);
    }

    ref<asset::MemoryMount> mount = createRef<asset::MemoryMount>();
    asset::Vfs vfs;
    gfx::FeatureRegistry registry;
};

constexpr std::string_view kBase = R"({
  "schema": 1,
  "name": "Test",
  "renderPath": "forward",
  "targets": { "color": ["surface"], "depth": "depth24plus", "samples": 1 },
  "features": [{ "id": "forward", "settings": { "shader": "cube" } }],
  "extensionPoints": { "opaque": ["forward"] },
  "fallbacks": []
})";

class TestFeature final : public gfx::RenderFeature {
public:
    explicit TestFeature(gfx::CompiledFeatureConfig config)
        : config_(std::move(config)) {}

    StringId Id() const noexcept override {
        return config_.feature;
    }

    const std::optional<std::string>& Instance() const noexcept override {
        return config_.instance;
    }

    const gfx::CompiledFeatureConfig& Config() const noexcept override {
        return config_;
    }

private:
    gfx::CompiledFeatureConfig config_;
};

class TestFactory final : public gfx::RenderFeatureFactory {
public:
    explicit TestFactory(gfx::RenderFeatureMetadata metadata)
        : metadata_(std::move(metadata)) {}

    const gfx::RenderFeatureMetadata& Metadata() const noexcept override {
        return metadata_;
    }

    Result<gfx::CompiledFeatureConfig> Compile(const std::map<std::string, gfx::ConfigValue, std::less<>>& values) const override {
        if (!values.empty())
            return Err(ErrorCode::InvalidArgument, "test feature has no settings");
        gfx::CompiledFeatureConfig config{metadata_.id, std::nullopt, metadata_.version, {}, {}};
        config.hash = gfx::HashFeatureConfig(config);
        return Ok(std::move(config));
    }

    Result<ref<const gfx::RenderFeature>> Create(const gfx::CompiledFeatureConfig& config, const gfx::FeatureServices&) const override {
        return Ok(ref<const gfx::RenderFeature>(createRef<TestFeature>(config)));
    }

private:
    gfx::RenderFeatureMetadata metadata_;
};

class EchoFactory final : public gfx::RenderFeatureFactory {
public:
    explicit EchoFactory(gfx::RenderFeatureMetadata metadata)
        : metadata_(std::move(metadata)) {}

    const gfx::RenderFeatureMetadata& Metadata() const noexcept override {
        return metadata_;
    }

    Result<gfx::CompiledFeatureConfig> Compile(const std::map<std::string, gfx::ConfigValue, std::less<>>& values) const override {
        gfx::CompiledFeatureConfig config{metadata_.id, std::nullopt, metadata_.version, {values.begin(), values.end()}, {}};
        config.hash = gfx::HashFeatureConfig(config);
        return Ok(std::move(config));
    }

    Result<ref<const gfx::RenderFeature>> Create(const gfx::CompiledFeatureConfig& config, const gfx::FeatureServices&) const override {
        return Ok(ref<const gfx::RenderFeature>(createRef<TestFeature>(config)));
    }

private:
    gfx::RenderFeatureMetadata metadata_;
};

class DeclaringFeature final : public gfx::RenderFeature {
public:
    DeclaringFeature(std::string name, ref<std::vector<std::string>> order, bool publish, bool fail = false)
        : name_(std::move(name)),
          order_(std::move(order)),
          publish_(publish),
          fail_(fail) {
        config_.feature = StringId(name_);
    }

    StringId Id() const noexcept override {
        return config_.feature;
    }

    const std::optional<std::string>& Instance() const noexcept override {
        return instance_;
    }

    const gfx::CompiledFeatureConfig& Config() const noexcept override {
        return config_;
    }

    Result<void> DeclareGraph(gfx::GraphDeclarationContext& context) const override {
        order_->push_back(name_);
        if (publish_)
            TRY_VOID(context.blackboard.Emplace<int>(42));
        else if (context.blackboard.Get<int>() == nullptr)
            return Err(ErrorCode::ValidationInvalidState, "missing blackboard exchange");
        return fail_ ? Err(ErrorCode::InvalidState, "declaration failed") : Ok();
    }

private:
    std::string name_;
    ref<std::vector<std::string>> order_;
    bool publish_{};
    bool fail_{};
    std::optional<std::string> instance_;
    gfx::CompiledFeatureConfig config_;
};

gfx::RenderFeatureMetadata TestMetadata(std::string name) {
    gfx::RenderFeatureMetadata metadata;
    metadata.id = StringId(name);
    metadata.debug_name = std::move(name);
    metadata.compatible_render_paths = {StringId("forward")};
    metadata.insertion_points = {StringId("opaque")};
    return metadata;
}

gfx::RenderPipelineIR Compile(Fixture& fixture, std::string_view source = kBase) {
    fixture.Put("test.woki-pipeline", source);
    auto result = gfx::RenderPipelineCompiler(fixture.vfs, fixture.registry).Compile(Path("test.woki-pipeline"));
    const std::string diagnostic = result.diagnostics.empty() ? "" : result.diagnostics.front().message;
    INFO(diagnostic);
    REQUIRE(result.pipeline);
    return std::move(*result.pipeline);
}

} // namespace

TEST_CASE("Pipeline JSONC is strict and reports stable pointers") {
    const auto path = Path("bad.woki-pipeline");
    auto comments = gfx::ParsePipeline(path, std::string("// comment\n") + std::string(kBase));
    CHECK(comments.Valid());
    auto unknown = gfx::ParsePipeline(path, R"({"schema":1,"name":"x","renderPath":"forward","targets":{},"features":[],"mystery":1})");
    REQUIRE_FALSE(unknown.Valid());
    CHECK(unknown.diagnostics.front().code == "PIP1003");
    auto wrong = gfx::ParsePipeline(path, R"({"schema":1,"name":2,"renderPath":[],"targets":[],"features":{}})");
    CHECK_FALSE(wrong.Valid());
    CHECK(std::ranges::any_of(wrong.diagnostics, [](const auto& item) { return item.pointer == "/name"; }));
}

TEST_CASE("Pipeline targets parse as typed formats with constrained samples") {
    const auto path = Path("targets.woki-pipeline");
    auto valid = gfx::ParsePipeline(path, kBase);
    REQUIRE(valid.Valid());
    CHECK(valid.value.targets.color == std::vector{gfx::PipelineTargetFormat::Surface});
    CHECK(valid.value.targets.depth == gfx::PipelineTargetFormat::Depth24Plus);
    CHECK_FALSE(gfx::ToRhiTextureFormat(gfx::PipelineTargetFormat::Surface));
    CHECK(gfx::ToRhiTextureFormat(gfx::PipelineTargetFormat::Depth24Plus) == rhi::TextureFormat::Depth24Plus);
    auto invalid = gfx::ParsePipeline(path, R"({"schema":1,"name":"x","renderPath":"forward","targets":{"color":["depth24plus"],"samples":3},"features":[]})");
    CHECK_FALSE(invalid.Valid());
    for (const u32 samples : {2u, 8u}) {
        const auto source = std::string(R"({"schema":1,"name":"x","renderPath":"forward","targets":{"color":["surface"],"samples":)") + std::to_string(samples) + R"(},"features":[]})";
        auto unsupported = gfx::ParsePipeline(path, source);
        CHECK_FALSE(unsupported.Valid());
        CHECK(std::ranges::any_of(unsupported.diagnostics, [](const auto& diagnostic) { return diagnostic.code == "PIP1012"; }));
    }

    Fixture fixture;
    const auto repeated = Compile(
        fixture,
        R"({"schema":1,"name":"mrt","renderPath":"forward","targets":{"color":["rgba8unorm","rgba8unorm"],"depth":"depth24plus"},"features":[{"id":"forward","settings":{"shader":"cube"}}],"extensionPoints":{"opaque":["forward"]},"fallbacks":[]})"
    );
    REQUIRE(repeated.targets.color.size() == 2);
    CHECK(repeated.targets.color[0] == repeated.targets.color[1]);
    auto bytes = gfx::SerializePipeline(repeated);
    REQUIRE(bytes);
    auto restored = gfx::ParsePipelineProduct(*bytes);
    REQUIRE(restored);
    CHECK(restored->targets.color == repeated.targets.color);
    for (const u32 samples : {2u, 8u}) {
        auto unsupported = repeated;
        unsupported.targets.samples = samples;
        unsupported.content_hash = gfx::HashPipeline(unsupported);
        CHECK_FALSE(gfx::SerializePipeline(unsupported));
    }
}

TEST_CASE("Feature config schemas reject unknown keys and wrong types") {
    Fixture fixture;
    auto unknown = gfx::ParseFeatureConfig(Path("bad.woki-feature"), R"({"schema":1,"feature":"shadows","settings":{},"run":"code"})");
    CHECK_FALSE(unknown.Valid());
    auto wrong = fixture.registry.Compile(StringId("shadows"), {{"resolution", std::string("large")}});
    CHECK_FALSE(wrong);
    auto valid = fixture.registry.Compile(StringId("shadows"), {{"resolution", i64{1024}}});
    REQUIRE(valid);
    CHECK(valid->values.size() == 2);
}

TEST_CASE("Runtime rejects schema-invalid deserialized feature settings from self-consistent factories") {
    Fixture fixture;
    auto metadata = TestMetadata("echo");
    metadata.config.fields = {{"value", gfx::ConfigType::String, true, std::nullopt}};
    REQUIRE(fixture.registry.Register(createRef<EchoFactory>(metadata)));
    const auto source = R"({"schema":1,"name":"echo","renderPath":"forward","targets":{"color":["surface"]},"features":[{"id":"echo","settings":{"value":"valid"}}],"extensionPoints":{"opaque":["echo"]},"fallbacks":[]})";
    const auto pipeline = Compile(fixture, source);

    const auto rejected = [&](std::vector<std::pair<std::string, gfx::ConfigValue>> values) {
        auto invalid = pipeline;
        invalid.features[0].config.values = std::move(values);
        invalid.features[0].config.hash = gfx::HashFeatureConfig(invalid.features[0].config);
        invalid.content_hash = gfx::HashPipeline(invalid);
        auto product = gfx::MakePipelineProduct(invalid);
        REQUIRE(product);
        gfx::RenderPipelineLibrary library;
        const auto handle = library.Create();
        REQUIRE(library.Publish(handle, *product));
        CHECK_FALSE(library.Compose(handle, {}, fixture.registry));
    };

    rejected({{"unknown", std::string("valid")}, {"value", std::string("valid")}});
    rejected({{"value", i64{1}}});
}

TEST_CASE("Feature registry is explicit and duplicate safe") {
    gfx::FeatureRegistry registry;
    REQUIRE(registry.RegisterExtensionPoint("opaque"));
    auto factory = createRef<TestFactory>(TestMetadata("test"));
    REQUIRE(registry.Register(factory));
    CHECK_FALSE(registry.Register(factory));
    REQUIRE(registry.RegisterCapability("known"));
    CHECK_FALSE(registry.RegisterCapability("known"));
    REQUIRE(registry.RegisterExtensionPoint("point"));
    CHECK_FALSE(registry.RegisterExtensionPoint("point"));
}

TEST_CASE("Feature registry rejects malformed metadata before publication") {
    gfx::FeatureRegistry registry;
    REQUIRE(registry.RegisterExtensionPoint("opaque"));
    auto mismatched = TestMetadata("debug-name");
    mismatched.id = StringId("other");
    CHECK_FALSE(registry.Register(createRef<TestFactory>(mismatched)));
    auto unknown_capability = TestMetadata("unknown-capability");
    unknown_capability.required_capabilities = {StringId("missing")};
    CHECK_FALSE(registry.Register(createRef<TestFactory>(unknown_capability)));
    auto bad_schema = TestMetadata("bad-schema");
    bad_schema.config.fields = {{"duplicate", gfx::ConfigType::String, false, std::nullopt}, {"duplicate", gfx::ConfigType::String, false, std::nullopt}};
    CHECK_FALSE(registry.Register(createRef<TestFactory>(bad_schema)));
}

TEST_CASE("Feature config hashing is typed and length prefixed") {
    gfx::CompiledFeatureConfig first{StringId("feature"), std::nullopt, {1, 0, 0}, {{"a", std::string("x|b=3:y")}}, {}};
    gfx::CompiledFeatureConfig second{StringId("feature"), std::nullopt, {1, 0, 0}, {{"a", std::string("x")}, {"b", std::string("y")}}, {}};
    CHECK(gfx::HashFeatureConfig(first) != gfx::HashFeatureConfig(second));
    second.instance = "stable";
    CHECK(gfx::HashFeatureConfig(first) != gfx::HashFeatureConfig(second));
}

TEST_CASE("Multiple features preserve unique stable instances through products and runtime") {
    Fixture fixture;
    auto metadata = TestMetadata("layer");
    metadata.multiplicity = gfx::FeatureMultiplicity::Multiple;
    REQUIRE(fixture.registry.Register(createRef<TestFactory>(metadata)));
    CHECK_FALSE(fixture.registry.Compile(metadata.id, {}));
    REQUIRE(fixture.registry.Compile(metadata.id, {}, "direct"));
    const auto source =
        R"({"schema":1,"name":"instances","renderPath":"forward","targets":{"color":["surface"]},"features":[{"id":"layer","instance":"left"},{"id":"layer","instance":"right"}],"extensionPoints":{"opaque":[{"id":"layer","instance":"left"},{"id":"layer","instance":"right"}]},"fallbacks":[]})";
    auto pipeline = Compile(fixture, source);
    REQUIRE(pipeline.features.size() == 2);
    CHECK(pipeline.features[0].instance == "left");
    CHECK(pipeline.features[1].instance == "right");
    auto product = gfx::MakePipelineProduct(pipeline);
    REQUIRE(product);
    auto parsed = gfx::ParsePipelineProduct(product->payload);
    REQUIRE(parsed);
    CHECK(parsed->features[1].instance == "right");
    gfx::RenderPipelineLibrary library;
    auto handle = library.Create();
    REQUIRE(library.Publish(handle, *product));
    auto instance = library.Compose(handle, {}, fixture.registry);
    REQUIRE(instance);
    CHECK(instance->features[0]->Instance() == std::optional<std::string>("left"));
}

TEST_CASE("Feature conflicts, dependency cycles, and unknown capabilities are rejected") {
    Fixture fixture;
    auto a = TestMetadata("a");
    auto b = TestMetadata("b");
    a.required_features = {b.id};
    b.required_features = {a.id};
    a.conflicting_features = {b.id};
    REQUIRE(fixture.registry.Register(createRef<TestFactory>(a)));
    REQUIRE(fixture.registry.Register(createRef<TestFactory>(b)));
    fixture.Put("cycle.woki-pipeline", R"({"schema":1,"name":"x","renderPath":"forward","targets":{"color":["surface"]},"features":[{"id":"a"},{"id":"b"}],"extensionPoints":{"opaque":["a","b"]},"fallbacks":[]})");
    const auto result = gfx::RenderPipelineCompiler(fixture.vfs, fixture.registry).Compile(Path("cycle.woki-pipeline"));
    CHECK_FALSE(result.pipeline);
    CHECK(std::ranges::any_of(result.diagnostics, [](const auto& item) { return item.code == "PIP2008"; }));
    CHECK(std::ranges::any_of(result.diagnostics, [](const auto& item) { return item.code == "PIP2012"; }));
}

TEST_CASE("Pipeline compiler reports bad references and missing feature dependencies") {
    Fixture fixture;
    fixture.Put("bad.woki-pipeline", R"({"schema":1,"name":"x","renderPath":"forward","targets":{"color":["surface"]},"features":[{"id":"shadows","config":"missing.woki-feature"}],"fallbacks":[]})");
    const auto result = gfx::RenderPipelineCompiler(fixture.vfs, fixture.registry).Compile(Path("bad.woki-pipeline"));
    CHECK_FALSE(result.pipeline);
    CHECK(std::ranges::any_of(result.diagnostics, [](const auto& item) { return item.code == "PIP2007"; }));
    CHECK(std::ranges::any_of(result.diagnostics, [](const auto& item) { return item.code == "PIP2013"; }));
}

TEST_CASE("Quality overrides settings and feature enablement deterministically") {
    Fixture fixture;
    fixture.Put("q.woki-quality", R"({"schema":1,"name":"q","features":{"bloom":{"enabled":true,"settings":{"intensity":0.25}}}})");
    const auto source = R"({"schema":1,"name":"x","renderPath":"forward","targets":{"color":["surface"]},"features":[{"id":"forward"}],"qualityProfile":"q.woki-quality","fallbacks":[]})";
    auto first = Compile(fixture, source);
    auto second = Compile(fixture, source);
    REQUIRE(first.features.size() == 2);
    CHECK(first.features[0].debug_name == "forward");
    CHECK(first.features[1].debug_name == "bloom");
    CHECK(first.content_hash == second.content_hash);
    CHECK(std::get<f64>(first.features[1].config.values[0].second) == 0.25);
}

TEST_CASE("Settings precedence is referenced config then inline then quality") {
    Fixture fixture;
    fixture.Put("forward.woki-feature", R"({"schema":1,"feature":"forward","settings":{"shader":"config"}})");
    fixture.Put("q.woki-quality", R"({"schema":1,"name":"q","features":{"forward":{"settings":{"shader":"quality"}}}})");
    auto pipeline = Compile(
        fixture,
        R"({"schema":1,"name":"precedence","renderPath":"forward","targets":{"color":["surface"]},"features":[{"id":"forward","config":"forward.woki-feature","settings":{"shader":"inline"}}],"qualityProfile":"q.woki-quality","fallbacks":[]})"
    );
    REQUIRE(pipeline.features.size() == 1);
    REQUIRE(pipeline.features[0].config.values.size() == 1);
    CHECK(std::get<std::string>(pipeline.features[0].config.values[0].second) == "quality");
}

TEST_CASE("Disabled quality overrides still validate feature IDs and setting keys") {
    Fixture fixture;
    fixture.Put("q.woki-quality", R"({"schema":1,"name":"q","features":{"forward":{"enabled":false,"settings":{"typo":true}},"missing":{"enabled":false}}})");
    fixture.Put("test.woki-pipeline", R"({"schema":1,"name":"x","renderPath":"forward","targets":{"color":["surface"]},"features":[{"id":"forward"}],"qualityProfile":"q.woki-quality","fallbacks":[]})");
    const auto result = gfx::RenderPipelineCompiler(fixture.vfs, fixture.registry).Compile(Path("test.woki-pipeline"));
    CHECK_FALSE(result.pipeline);
    CHECK(std::ranges::any_of(result.diagnostics, [](const auto& item) { return item.code == "PIP2022"; }));
    CHECK(std::ranges::any_of(result.diagnostics, [](const auto& item) { return item.code == "PIP2023"; }));
}

TEST_CASE("Enabled optional dependencies contribute deterministic DAG edges") {
    Fixture fixture;
    auto dependent = TestMetadata("a-dependent");
    auto optional = TestMetadata("z-optional");
    dependent.optional_features = {optional.id};
    REQUIRE(fixture.registry.Register(createRef<TestFactory>(dependent)));
    REQUIRE(fixture.registry.Register(createRef<TestFactory>(optional)));
    auto pipeline = Compile(fixture, R"({"schema":1,"name":"optional","renderPath":"forward","targets":{"color":["surface"]},"features":[{"id":"a-dependent"},{"id":"z-optional"}],"fallbacks":[]})");
    REQUIRE(pipeline.features.size() == 2);
    CHECK(pipeline.features[0].debug_name == "z-optional");
    CHECK(pipeline.features[1].debug_name == "a-dependent");
}

TEST_CASE("Extension constraints reject unknown points and disabled features") {
    Fixture fixture;
    const auto unknown = R"({"schema":1,"name":"x","renderPath":"forward","targets":{"color":["surface"]},"features":[{"id":"forward"}],"extensionPoints":{"unknown":["forward"],"opaque":["lighting"]},"fallbacks":[]})";
    fixture.Put("test.woki-pipeline", unknown);
    const auto result = gfx::RenderPipelineCompiler(fixture.vfs, fixture.registry).Compile(Path("test.woki-pipeline"));
    CHECK_FALSE(result.pipeline);
    CHECK(std::ranges::any_of(result.diagnostics, [](const auto& item) { return item.code == "PIP2009"; }));
    CHECK(std::ranges::any_of(result.diagnostics, [](const auto& item) { return item.code == "PIP2010"; }));
}

TEST_CASE("Multiplicity and fallback cycles are rejected") {
    Fixture fixture;
    fixture.Put("duplicate.woki-pipeline", R"({"schema":1,"name":"x","renderPath":"forward","targets":{"color":["surface"]},"features":[{"id":"forward"},{"id":"forward"}],"fallbacks":[]})");
    auto duplicate = gfx::RenderPipelineCompiler(fixture.vfs, fixture.registry).Compile(Path("duplicate.woki-pipeline"));
    CHECK_FALSE(duplicate.pipeline);
    CHECK(std::ranges::any_of(duplicate.diagnostics, [](const auto& item) { return item.code == "PIP2003"; }));
    fixture.Put("a.woki-pipeline",
        R"({"schema":1,"name":"a","renderPath":"forward","targets":{"color":["surface"]},"features":[{"id":"forward"}],"fallbacks":[{"pipeline":"b.woki-pipeline","missingCapabilities":["hdr"]}]})");
    fixture.Put("b.woki-pipeline",
        R"({"schema":1,"name":"b","renderPath":"forward","targets":{"color":["surface"]},"features":[{"id":"forward"}],"fallbacks":[{"pipeline":"a.woki-pipeline","missingCapabilities":["hdr"]}]})");
    auto cycle = gfx::RenderPipelineCompiler(fixture.vfs, fixture.registry).Compile(Path("a.woki-pipeline"));
    CHECK_FALSE(cycle.pipeline);
    CHECK(std::ranges::any_of(cycle.diagnostics, [](const auto& item) { return item.code == "PIP2017"; }));
}

TEST_CASE("Pipeline product bytes and hashes are deterministic and bounded") {
    Fixture fixture;
    auto pipeline = Compile(fixture);
    auto first = gfx::MakePipelineProduct(pipeline);
    auto second = gfx::MakePipelineProduct(pipeline);
    REQUIRE(first);
    REQUIRE(second);
    CHECK(first->payload == second->payload);
    CHECK(first->product_hash == second->product_hash);
    auto parsed = gfx::ParsePipelineProduct(first->payload);
    REQUIRE(parsed);
    CHECK(parsed->content_hash == pipeline.content_hash);
    REQUIRE(gfx::SerializePipeline(*parsed));
    CHECK(*gfx::SerializePipeline(*parsed) == first->payload);
    auto trailing = first->payload;
    trailing.push_back(std::byte{});
    CHECK_FALSE(gfx::ParsePipelineProduct(trailing));
    CHECK_FALSE(gfx::ParsePipelineProduct(first->payload, {.max_records = 0, .max_string_bytes = 4}));
    auto corrupt = first->payload;
    corrupt[12] ^= std::byte{1};
    CHECK_FALSE(gfx::ParsePipelineProduct(corrupt));
    CHECK(first->source_hash == pipeline.root_source_hash);
    CHECK(parsed->root_source_hash == pipeline.root_source_hash);
    CHECK(std::ranges::any_of(pipeline.dependencies, [&](const auto& dependency) { return dependency.path == Path("test.woki-pipeline") && dependency.hash == pipeline.root_source_hash; }));
}

TEST_CASE("Pipeline products bind the root source hash to the root dependency identity") {
    Fixture fixture;
    fixture.Put("fallback.woki-pipeline", kBase);
    const auto
        source = R"({"schema":1,"name":"root","renderPath":"forward","targets":{"color":["surface"]},"features":[{"id":"forward"}],"fallbacks":[{"pipeline":"fallback.woki-pipeline","missingCapabilities":["hdr"]}]})";
    auto pipeline = Compile(fixture, source);
    const auto root = std::ranges::find_if(pipeline.dependencies, [&](const auto& dependency) { return asset::AssetId::FromName("engine://" + dependency.path.String()) == pipeline.asset_id; });
    const auto dependency = std::ranges::find_if(pipeline.dependencies, [&](const auto& item) { return asset::AssetId::FromName("engine://" + item.path.String()) != pipeline.asset_id; });
    REQUIRE(root != pipeline.dependencies.end());
    REQUIRE(dependency != pipeline.dependencies.end());
    std::swap(root->hash, dependency->hash);
    pipeline.content_hash = gfx::HashPipeline(pipeline);
    CHECK_FALSE(gfx::SerializePipeline(pipeline));
}

TEST_CASE("Pipeline hashing covers typed targets, instances, and capability names") {
    Fixture fixture;
    auto pipeline = Compile(fixture);
    auto changed = pipeline;
    changed.targets.samples = 4;
    CHECK(gfx::HashPipeline(changed) != pipeline.content_hash);
    changed = pipeline;
    changed.features[0].instance = "identity";
    CHECK(gfx::HashPipeline(changed) != pipeline.content_hash);
    changed = pipeline;
    changed.required_capability_names.push_back("a|b:c");
    CHECK(gfx::HashPipeline(changed) != pipeline.content_hash);
}

TEST_CASE("Runtime composition retains generations and rejects failed reloads") {
    Fixture fixture;
    auto pipeline = Compile(fixture);
    auto product = gfx::MakePipelineProduct(pipeline);
    REQUIRE(product);
    gfx::RenderPipelineLibrary library;
    const auto handle = library.Create();
    REQUIRE(library.Publish(handle, *product));
    auto old = library.Borrow(handle);
    REQUIRE(old);
    auto instance = library.Compose(handle, {StringId("depth-texture")}, fixture.registry);
    REQUIRE(instance);
    REQUIRE(instance->features.size() == 1);
    CHECK(instance->features[0]->Id() == StringId("forward"));
    REQUIRE(library.Publish(handle, *product));
    CHECK(old->Version() == 1);
    CHECK(library.Borrow(handle)->Version() == 2);
    gfx::PipelineDependencyGraph graph;
    gfx::PipelineReloadCoordinator reload(library, graph);
    auto malformed = *product;
    malformed.payload.push_back(std::byte{});
    malformed.product_hash = asset::HashProduct(malformed);
    auto rejected = reload.Publish({handle, 2, Result<asset::Product>{malformed}});
    CHECK(rejected.outcome == gfx::PipelineReloadOutcome::Rejected);
    CHECK(library.Borrow(handle)->Version() == 2);
    auto stale = reload.Publish({handle, 1, Result<asset::Product>{*product}});
    CHECK(stale.outcome == gfx::PipelineReloadOutcome::Stale);

    REQUIRE(library.Destroy(handle));
    CHECK_FALSE(library.Borrow(handle));
    const auto reused = library.Create();
    CHECK(reused.Index() == handle.Index());
    CHECK(reused.Generation() != handle.Generation());
    CHECK_FALSE(library.Publish(handle, *product));
}

TEST_CASE("Pipeline instances declare graphs in canonical order with one blackboard") {
    auto order = createRef<std::vector<std::string>>();
    gfx::PipelineInstance instance;
    instance.features.push_back(createRef<DeclaringFeature>("publish", order, true));
    instance.features.push_back(createRef<DeclaringFeature>("consume", order, false));
    gfx::RenderGraphBuilder graph;
    REQUIRE(gfx::DeclarePipelineGraph(instance, graph, 64, 32));
    CHECK(*order == std::vector<std::string>{"publish", "consume"});
    REQUIRE(graph.Blackboard().Get<int>() != nullptr);
    CHECK(*graph.Blackboard().Get<int>() == 42);

    instance.features.push_back(createRef<DeclaringFeature>("fail", order, false, true));
    CHECK_FALSE(gfx::DeclarePipelineGraph(instance, graph, 64, 32));
}

TEST_CASE("Fallback selection uses normalized capabilities and detects unresolved targets") {
    Fixture fixture;
    fixture.Put("fallback.woki-pipeline", kBase);
    const auto root_source =
        R"({"schema":1,"name":"root","renderPath":"forward","targets":{"color":["surface"]},"features":[{"id":"forward"}],"fallbacks":[{"pipeline":"fallback.woki-pipeline","missingCapabilities":["hdr"]}]})";
    fixture.Put("root.woki-pipeline", root_source);
    auto root_ir = gfx::RenderPipelineCompiler(fixture.vfs, fixture.registry).Compile(Path("root.woki-pipeline"));
    auto fallback_ir = gfx::RenderPipelineCompiler(fixture.vfs, fixture.registry).Compile(Path("fallback.woki-pipeline"));
    REQUIRE(root_ir.pipeline);
    REQUIRE(fallback_ir.pipeline);
    auto root_product = gfx::MakePipelineProduct(*root_ir.pipeline);
    auto fallback_product = gfx::MakePipelineProduct(*fallback_ir.pipeline);
    gfx::RenderPipelineLibrary library;
    auto root = library.Create();
    REQUIRE(library.Publish(root, *root_product));
    CHECK_FALSE(library.SelectSupported(root, {}));
    auto fallback = library.Create();
    REQUIRE(library.Publish(fallback, *fallback_product));
    CHECK(library.SelectSupported(root, {StringId("depth-texture")})->Get().debug_name == "Test");
    CHECK(library.SelectSupported(root, {StringId("hdr"), StringId("hdr")})->Get().debug_name == "root");
}

TEST_CASE("All shipped render pipelines compile and cook") {
    auto mount = asset::DirectoryMount::Create(std::filesystem::path(WOKI_GFX_TEST_ASSET_ROOT));
    REQUIRE(mount);
    asset::Vfs vfs;
    vfs.AddMount(*mount);
    gfx::FeatureRegistry registry;
    REQUIRE(gfx::RegisterStandardFeatures(registry));
    for (const std::string name : {"compatibility-forward", "mobile-forward", "desktop-forward", "editor-forward", "cube"}) {
        std::vector<gfx::PipelineDiagnostic> diagnostics;
        auto product = gfx::CookPipeline(vfs, registry, Path("render/" + name + ".woki-pipeline"), &diagnostics);
        INFO(name);
        const std::string diagnostic = diagnostics.empty() ? "" : diagnostics.front().message;
        INFO(diagnostic);
        CHECK(product);
    }
}

TEST_CASE("Shared source selection follows the shipped transitive fallback chain") {
    auto mount = asset::DirectoryMount::Create(std::filesystem::path(WOKI_GFX_TEST_ASSET_ROOT));
    REQUIRE(mount);
    asset::Vfs vfs;
    vfs.AddMount(*mount);
    gfx::FeatureRegistry registry;
    REQUIRE(gfx::RegisterStandardFeatures(registry));
    const auto desktop = Path("render/desktop-forward.woki-pipeline");
    auto compatibility = gfx::SelectSupportedPipeline(vfs, registry, desktop, {});
    REQUIRE(compatibility);
    CHECK(compatibility->String() == "render/compatibility-forward.woki-pipeline");
    auto mobile = gfx::SelectSupportedPipeline(vfs, registry, desktop, {StringId("depth-texture")});
    REQUIRE(mobile);
    CHECK(mobile->String() == "render/mobile-forward.woki-pipeline");
    auto selected_desktop = gfx::SelectSupportedPipeline(vfs, registry, desktop, {StringId("hdr"), StringId("depth-texture")});
    REQUIRE(selected_desktop);
    CHECK(*selected_desktop == desktop);
}
