#include <filesystem>

#include <catch2/catch_test_macros.hpp>

#include <woki/gfx/advanced.hpp>

using namespace woki;

TEST_CASE("climbing FBX imports cooks and evaluates its recorded animation") {
    const auto root = std::filesystem::path(WOKI_GFX_TEST_ASSET_ROOT);
    auto mount = asset::DirectoryMount::Create(root);
    REQUIRE(mount);
    auto vfs = createRef<asset::Vfs>();
    REQUIRE(vfs->MountAt("climbing-test", asset::AssetScheme::Engine, {}, 0, *mount));
    const auto descriptor_uri = *asset::AssetUri::Parse("engine://anim/climbing.woki-mesh-import");
    const auto source_uri = *asset::AssetUri::Parse("engine://anim/climbing.fbx");
    auto descriptor_text = vfs->ReadText(descriptor_uri, 1024 * 1024);
    auto fbx = vfs->ReadBinary(source_uri, 1024U * 1024U * 1024U);
    REQUIRE(descriptor_text);
    REQUIRE(fbx);
    CHECK(fbx->size() > 100'000);

    auto source = gfx::ParseMeshSource(*descriptor_text);
    REQUIRE(source);
    CHECK(source->lod_ratios == std::vector<f32>{1.0F});
    CHECK_FALSE(source->meshlets.enabled);
    auto importers = gfx::CreateDefaultModelImporterRegistry();
    REQUIRE(importers);
    const auto* importer = importers->Select(".fbx", {}, "assimp");
    REQUIRE(importer != nullptr);
    auto imported = importer->Import({source_uri, {}, *fbx, [vfs](const asset::AssetUri& uri) { return vfs->ReadBinary(uri, 1024U * 1024U * 1024U); }, source->unit_scale});
    REQUIRE(imported);
    CHECK(imported->conversion.canonical_coordinate_system == "right-handed,+Y-up,-Z-forward");
    CHECK(imported->conversion.applied_scale > 0.0F);
    REQUIRE_FALSE(imported->meshes.empty());
    REQUIRE_FALSE(imported->skeletons.empty());
    REQUIRE_FALSE(imported->animations.empty());
    CHECK(std::ranges::all_of(imported->meshes, [](const auto& mesh) {
        return std::ranges::all_of(mesh.primitives, [](const auto& primitive) { return primitive.topology == gfx::ImportedTopology::Triangles && !primitive.positions.empty() && !primitive.indices.empty(); });
    }));

    auto registry = createRef<const gfx::ModelImporterRegistry>(std::move(*importers));
    gfx::MeshBuilder builder(vfs, registry);
    asset::BuildContext context;
    auto product = builder.Build({source->asset_id, descriptor_uri, Sha256(*descriptor_text), {}, 1}, context, std::as_bytes(std::span(*descriptor_text)));
    REQUIRE(product);
    CHECK(product->type == gfx::kMeshProductType);
    CHECK(product->asset_id == source->asset_id);
    CHECK(std::ranges::any_of(context.SourceDependencies(), [&](const auto& dependency) { return dependency.asset_id == asset::AssetId::FromName(source_uri.String()); }));

    auto mesh = gfx::ParseMeshProduct(product->payload);
    REQUIRE(mesh);
    CHECK(mesh->importer == "assimp");
    REQUIRE(mesh->lods.size() == 1);
    for (const auto& lod : mesh->lods) {
        CHECK(lod.vertex_count > 0);
        CHECK(lod.index_count > 0);
        auto meshlets = gfx::DecodeMeshletStreams(mesh->Chunk(lod.meshlets));
        REQUIRE(meshlets);
        CHECK_FALSE(meshlets->descriptors.empty());
        CHECK(meshlets->descriptors.size() == meshlets->bounds.size());
    }
    REQUIRE_FALSE(mesh->skeleton.empty());
    REQUIRE_FALSE(mesh->animations.empty());
    CHECK(mesh->animations.front().duration > 0.0F);
    CHECK_FALSE(mesh->animations.front().channels.empty());

    gfx::SkeletonRegistry skeletons;
    gfx::AnimationClipRegistry clips;
    const auto skeleton_handles = skeletons.RegisterAll(*mesh, 7);
    const auto clip_handles = clips.Register(*mesh, 7);
    REQUIRE_FALSE(skeleton_handles.empty());
    REQUIRE_FALSE(clip_handles.empty());
    auto skeleton = skeletons.Get(skeleton_handles.front());
    auto clip = clips.Get(clip_handles.front());
    REQUIRE(skeleton);
    REQUIRE(clip);
    gfx::Animator animator(skeleton);
    animator.SetClips({clip});
    REQUIRE(animator.Select(0));
    animator.Update(clip->duration * 0.5F);
    auto palette = animator.Evaluate();
    REQUIRE(palette);
    CHECK(palette->size() == skeleton->joints.size());
    gfx::SkinPalette skin(static_cast<u32>(palette->size()));
    REQUIRE(skin.Set(*palette, 7));
    CHECK(skin.ContentVersion() == 2);
}
