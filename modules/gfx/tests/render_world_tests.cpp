#include <catch2/catch_test_macros.hpp>

#include <woki/gfx/advanced.hpp>

using namespace woki;

namespace {

gfx::RenderObjectData Object(math::vec3f center, u64 mask = ~u64{0}) {
    gfx::RenderObjectData value;
    value.bounds = {.center = center, .radius = 0.1F, .minimum = center - math::vec3f(0.1F), .maximum = center + math::vec3f(0.1F)};
    value.visibility_mask = mask;
    value.lod.count = 3;
    value.lod.geometric_errors = {0.0F, 0.01F, 0.1F};
    value.lod.last_resident = 2;
    return value;
}

gfx::RenderView View(gfx::SceneHandle scene) {
    gfx::RenderView view;
    view.scene = scene;
    view.camera.view = math::mat4f::identity();
    view.camera.projection = math::mat4f::identity();
    view.camera.view_projection = math::mat4f::identity();
    view.viewport = {0, 0, 256, 256};
    return view;
}

} // namespace

TEST_CASE("RenderWorld coalesces mutations while retaining previous immutable snapshots") {
    const auto scene_id = gfx::SceneHandle::Create(1, 1);
    gfx::RenderScene scene(scene_id, 0x1000);
    auto writer = scene.CreateWriter();
    auto id = scene.CreateObject(writer, Object({0.0F, 0.0F, 0.5F}));
    REQUIRE(id);
    REQUIRE(scene.UpdateObject(writer, *id, 2, {.visibility_mask = 0x2}));
    REQUIRE(scene.UpdateObject(writer, *id, 3, {.layers = 0x4}));
    REQUIRE(writer.Flush());
    auto changes = scene.FreezeAndDrain();
    REQUIRE(changes);

    gfx::RenderWorldBuilder world(scene_id);
    const auto empty = world.Current();
    auto snapshot = world.Apply(*changes);
    REQUIRE(snapshot);
    REQUIRE((*snapshot)->ObjectData().ids.size() == 1);
    CHECK((*snapshot)->ObjectData().versions[0] == 3);
    CHECK((*snapshot)->ObjectData().visibility_masks[0] == 0x2);
    CHECK((*snapshot)->ObjectData().layers[0] == 0x4);
    CHECK(empty->ObjectData().ids.empty());
}

TEST_CASE("RenderWorld rejects mixed scenes and keeps identical local IDs isolated") {
    const auto scene_a = gfx::SceneHandle::Create(1, 1);
    const auto scene_b = gfx::SceneHandle::Create(2, 1);
    const auto object = gfx::RenderObjectId::Create(5, 1);
    const gfx::RenderChange a{scene_a, 1, 1, gfx::RenderObjectChange{gfx::SceneChangeKind::Create, object, 1, Object({}), {}}};
    const gfx::RenderChange b{scene_b, 2, 1, gfx::RenderObjectChange{gfx::SceneChangeKind::Create, object, 1, Object({}), {}}};
    gfx::RenderWorldBuilder mixed(scene_a);
    const std::array changes{a, b};
    CHECK_FALSE(mixed.Apply(changes));

    gfx::RenderWorldBuilder left(scene_a);
    gfx::RenderWorldBuilder right(scene_b);
    REQUIRE(left.Apply(std::span(&a, 1)));
    REQUIRE(right.Apply(std::span(&b, 1)));
    CHECK(left.Current()->Scene() != right.Current()->Scene());
    CHECK(left.Current()->Resolve(object) == 0);
    CHECK(right.Current()->Resolve(object) == 0);
}

TEST_CASE("Visibility is deterministic across batching and applies masks frustum and LOD residency") {
    const auto scene_id = gfx::SceneHandle::Create(1, 1);
    gfx::RenderScene scene(scene_id, 0x2000);
    auto writer = scene.CreateWriter();
    auto visible = scene.CreateObject(writer, Object({0.0F, 0.0F, 0.5F}, 0x1));
    REQUIRE(visible);
    REQUIRE(scene.CreateObject(writer, Object({4.0F, 0.0F, 0.5F}, 0x1)));
    REQUIRE(scene.CreateObject(writer, Object({0.0F, 0.0F, 0.5F}, 0x2)));
    REQUIRE(writer.Flush());
    auto changes = scene.FreezeAndDrain();
    REQUIRE(changes);
    gfx::RenderWorldBuilder world(scene_id);
    auto snapshot = world.Apply(*changes);
    REQUIRE(snapshot);
    auto view = View(scene_id);
    view.visibility_mask = 0x1;
    gfx::VisibilityService visibility_service;
    auto serial = visibility_service.Cull(**snapshot, view, {.lod_error_pixels = 1.0F, .batch_size = 1});
    auto grouped = visibility_service.Cull(**snapshot, view, {.lod_error_pixels = 1.0F, .batch_size = 64});
    REQUIRE(serial);
    REQUIRE(grouped);
    REQUIRE(serial->visible.size() == 1);
    CHECK(serial->visible.front().id == *visible);
    CHECK(serial->visible.front().lod == 0);
    REQUIRE(serial->visible.size() == grouped->visible.size());
    for (size_t index = 0; index < serial->visible.size(); ++index) {
        CHECK(serial->visible[index].id == grouped->visible[index].id);
        CHECK(serial->visible[index].lod == grouped->visible[index].lod);
        CHECK(serial->visible[index].phase == grouped->visible[index].phase);
    }
}

TEST_CASE("CPU visibility agrees with the GPU candidate contract deterministically") {
    const auto scene_id = gfx::SceneHandle::Create(3, 1);
    gfx::RenderScene scene(scene_id, 0x3000);
    auto writer = scene.CreateWriter();
    for (const auto center : {math::vec3f{0.0F, 0.0F, 0.5F}, math::vec3f{0.75F, 0.0F, 0.5F}, math::vec3f{2.0F, 0.0F, 0.5F}})
        REQUIRE(scene.CreateObject(writer, Object(center, 0x5)));
    REQUIRE(writer.Flush());
    auto changes = scene.FreezeAndDrain();
    REQUIRE(changes);
    gfx::RenderWorldBuilder world(scene_id);
    auto snapshot = world.Apply(*changes);
    REQUIRE(snapshot);
    const auto view = View(scene_id);
    auto cpu = gfx::VisibilityService{}.Cull(**snapshot, view, {.lod_hysteresis = 0.0F});
    REQUIRE(cpu);

    const auto frustum = gfx::Frustum::FromWebGpuViewProjection(view.camera.view_projection);
    std::vector<gfx::RenderObjectId> gpu_contract;
    const auto& objects = (*snapshot)->ObjectData();
    for (u32 index = 0; index < objects.ids.size(); ++index) {
        gfx::GpuVisibilityCandidate candidate;
        candidate.sphere = {objects.bounds[index].center.x, objects.bounds[index].center.y, objects.bounds[index].center.z, objects.bounds[index].radius};
        candidate
            .masks = {static_cast<u32>(objects.visibility_masks[index]), static_cast<u32>(objects.visibility_masks[index] >> 32U), static_cast<u32>(view.visibility_mask), static_cast<u32>(view.visibility_mask >> 32U)};
        const bool mask_visible = ((u64{candidate.masks[1]} << 32U) | candidate.masks[0]) & ((u64{candidate.masks[3]} << 32U) | candidate.masks[2]);
        if (mask_visible && frustum.IntersectsSphere(objects.bounds[index].center, objects.bounds[index].radius) && frustum.IntersectsAabb(objects.bounds[index].minimum, objects.bounds[index].maximum))
            gpu_contract.push_back(objects.ids[index]);
    }
    CHECK(cpu->visible.size() == gpu_contract.size());
    for (size_t index = 0; index < gpu_contract.size(); ++index)
        CHECK(cpu->visible[index].id == gpu_contract[index]);
}

TEST_CASE("RenderWorld coalesces multiple view updates without crossing scene boundaries") {
    const auto scene_id = gfx::SceneHandle::Create(9, 1);
    gfx::RenderScene scene(scene_id, 0x9000);
    auto writer = scene.CreateWriter();
    auto left = scene.CreateView(writer, View(scene_id));
    auto right = scene.CreateView(writer, View(scene_id));
    REQUIRE(left);
    REQUIRE(right);
    auto moved = View(scene_id);
    moved.viewport = {16, 8, 128, 64};
    REQUIRE(scene.UpdateView(writer, *left, 2, moved));
    REQUIRE(writer.Flush());
    auto changes = scene.FreezeAndDrain();
    REQUIRE(changes);
    gfx::RenderWorldBuilder world(scene_id);
    auto snapshot = world.Apply(*changes);
    REQUIRE(snapshot);
    REQUIRE((*snapshot)->Views().size() == 2);
    const auto updated = std::ranges::find((*snapshot)->Views(), *left, &gfx::RenderView::id);
    const auto untouched = std::ranges::find((*snapshot)->Views(), *right, &gfx::RenderView::id);
    REQUIRE(updated != (*snapshot)->Views().end());
    REQUIRE(untouched != (*snapshot)->Views().end());
    CHECK(updated->viewport.x == 16);
    CHECK(untouched->viewport.x == 0);
}
