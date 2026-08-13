#include <filesystem>
#include <catch2/catch_test_macros.hpp>

#include <woki/gfx/advanced.hpp>

namespace {
woki::gfx::ShaderPack LoadStandardPack() {
    const auto mount = woki::asset::DirectoryMount::Create(WOKI_GFX_TEST_ASSET_ROOT);
    REQUIRE(mount);
    woki::asset::Vfs vfs;
    vfs.AddMount(*mount);
    const auto manifest = woki::asset::AssetPath::Parse("shaders/standard.woki-shader-pack");
    REQUIRE(manifest);
    auto pack = woki::gfx::LoadShaderPack(vfs, *manifest);
    REQUIRE(pack);
    return std::move(*pack);
}
} // namespace

TEST_CASE("Standard shader pack loads every manifest descriptor deterministically") {
    const auto first = LoadStandardPack();
    const auto second = LoadStandardPack();
    REQUIRE(first.entries.size() == 15);
    CHECK(first.id == "woki.standard");
    CHECK(first.version == "1.0.0");
    CHECK(first.content_hash == second.content_hash);
    std::size_t descriptor_count = 0;
    for (const auto& item : std::filesystem::directory_iterator(std::filesystem::path(WOKI_GFX_TEST_ASSET_ROOT) / "shaders/descriptors"))
        descriptor_count += item.path().extension() == ".woki-shader";
    CHECK(descriptor_count == first.entries.size());
    for (const auto& entry : first.entries) {
        CHECK_FALSE(entry.descriptor.entry_points.empty());
        CHECK_FALSE(entry.source.code.empty());
        CHECK_FALSE(entry.source.dependencies.empty());
        CHECK(entry.source.diagnostics.empty());
        CHECK(std::ranges::binary_search(entry.source.dependencies, entry.descriptor_path));
    }

    const auto index = woki::gfx::MakeShaderPackIndex(first);
    const auto bytes_a = woki::gfx::SerializeShaderPackIndex(index);
    const auto bytes_b = woki::gfx::SerializeShaderPackIndex(index);
    REQUIRE(bytes_a);
    REQUIRE(bytes_b);
    CHECK(*bytes_a == *bytes_b);
    const auto parsed = woki::gfx::ParseShaderPackIndex(*bytes_a);
    REQUIRE(parsed);
    CHECK(parsed->content_hash == first.content_hash);
    CHECK(parsed->entries.size() == first.entries.size());

    auto noncanonical = index;
    noncanonical.entries.front().dependencies.push_back(*woki::asset::AssetPath::Parse("shaders/changed-include.wgsl"));
    std::ranges::sort(noncanonical.entries.front().dependencies);
    CHECK_FALSE(woki::gfx::SerializeShaderPackIndex(noncanonical));
}

TEST_CASE("Standard shader validation state is explicit") {
    if (!woki::gfx::IsTintShaderCompilerAvailable()) {
        const auto pack = LoadStandardPack();
        const auto output = woki::gfx::CreateTintShaderCompiler()->Compile({pack.entries.front().descriptor, pack.entries.front().source});
        CHECK_FALSE(output.validated_with_tint);
        REQUIRE_FALSE(output.diagnostics.empty());
        CHECK(output.diagnostics.front().code == "SHD3001");
        return;
    }
    const auto pack = LoadStandardPack();
    const auto compiler = woki::gfx::CreateTintShaderCompiler();
    for (const auto& entry : pack.entries) {
        CAPTURE(entry.name);
        const auto output = compiler->Compile({entry.descriptor, entry.source});
        const std::string diagnostic = output.diagnostics.empty() ? "no diagnostic" : output.diagnostics.front().message;
        INFO(diagnostic);
        CHECK(output.diagnostics.empty());
        CHECK(output.validated_with_tint);
    }
}

TEST_CASE("Shader pack manifests reject unknown keys and wrong schema types") {
    auto mount = woki::createRef<woki::asset::MemoryMount>();
    const auto path = *woki::asset::AssetPath::Parse("pack.json");
    woki::asset::Vfs vfs;
    vfs.AddMount(mount);
    mount->PutText(path, R"({"schema":"1","id":"test","version":"1","shaders":[]})");
    CHECK_FALSE(woki::gfx::LoadShaderPack(vfs, path));
    mount->PutText(path, R"({"schema":1,"id":"test","version":"1","shaders":[],"unexpected":true})");
    CHECK_FALSE(woki::gfx::LoadShaderPack(vfs, path));
}
