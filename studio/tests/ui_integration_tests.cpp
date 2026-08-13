#include <algorithm>
#include <array>
#include <string>
#include <type_traits>
#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <woki/gfx/view.hpp>
#include <woki/rhi.hpp>
#include <woki/rhi/null.hpp>
#include <woki/ui/render.hpp>

#include "editor/dock_workspace.hpp"
#include "editor/platform_ui_adapter.hpp"
#include "editor/scene_panel.hpp"

namespace {

class SceneImages final : public woki::ui::render::ImageResolver {
public:
    std::optional<woki::gfx::CanvasImageSource> Resolve(woki::ui::ImageId image) const override {
        if (image == woki::studio::kSceneImage)
            return woki::gfx::OffscreenTargetHandle::Create(2, 1);
        return std::nullopt;
    }
};

bool Contains(std::span<const std::string> log, std::string_view value) {
    return std::ranges::find(log, value) != log.end();
}

} // namespace

TEST_CASE("platform events preserve modifier state pointer richness and composition") {
    using namespace woki;
    studio::PlatformUiAdapter adapter;

    REQUIRE(std::get<ui::KeyEvent>(*adapter.Convert(events::KeyPressedEvent{events::KeyCode::kLeftControl})).control);
    REQUIRE(std::get<ui::KeyEvent>(*adapter.Convert(events::KeyPressedEvent{events::KeyCode::kRightShift})).shift);
    REQUIRE(std::get<ui::KeyEvent>(*adapter.Convert(events::KeyPressedEvent{events::KeyCode::kLeftAlt})).alt);
    REQUIRE(std::get<ui::KeyEvent>(*adapter.Convert(events::KeyPressedEvent{events::KeyCode::kRightSuper})).super);

    events::PointerData source{.pointer = 42,
        .kind = events::PointerKind::kPen,
        .button = events::PointerButton::kPrimary,
        .buttons = 3,
        .x = 12,
        .y = 24,
        .delta_x = 2,
        .delta_y = -3,
        .pressure = 0.8f,
        .contact_width = 4,
        .contact_height = 6,
        .tilt_x = 10,
        .tilt_y = -5};
    const auto pointer = std::get<ui::PointerEvent>(*adapter.Convert(events::PointerDownEvent{source}));
    CHECK(pointer.pointer == 42);
    CHECK(pointer.kind == ui::PointerKind::Pen);
    CHECK(pointer.modifiers == 15);
    CHECK(pointer.pressure == Catch::Approx(0.8f));
    CHECK(pointer.contact_height == 6);
    CHECK(pointer.tilt_y == -5);

    const auto composition = std::get<ui::CompositionEvent>(*adapter
            .Convert(events::TextCompositionUpdatedEvent{"compose", 2, 3}));
    CHECK(composition.type == ui::CompositionEvent::Type::Update);
    CHECK(composition.text == "compose");
    CHECK(composition.selection_start == 2);
    CHECK(composition.selection_length == 3);

    const auto released = std::get<ui::KeyEvent>(*adapter
            .Convert(events::KeyReleasedEvent{events::KeyCode::kLeftControl}));
    CHECK_FALSE(released.control);
    CHECK(released.shift);
    adapter.Reset();
    CHECK_FALSE(std::get<ui::KeyEvent>(*adapter.Convert(events::KeyPressedEvent{events::KeyCode::kF})).shift);
}

TEST_CASE("platform to UI routing gives handled UI controls priority over scene input") {
    using namespace woki;
    studio::PlatformUiAdapter adapter;
    ui::Runtime runtime;
    int ui_calls = 0;
    int scene_calls = 0;
    runtime.SetContent(ui::Box().Size(60, 40).OnEvent([&](ui::EventContext& context, const ui::Event&) {
        if (context.phase == ui::Phase::Target) {
            ++ui_calls;
            context.Handle();
        }
    }));
    runtime.Prepare({.viewport = {60, 40}});

    events::PointerDownEvent event{{.pointer = 1,
        .kind = events::PointerKind::kMouse,
        .button = events::PointerButton::kPrimary,
        .x = 10,
        .y = 10}};
    if (auto converted = adapter.Convert(event); converted && runtime.HandleEvent(*converted))
        event.handled = true;
    else
        ++scene_calls;

    CHECK(event.handled);
    CHECK(ui_calls == 1);
    CHECK(scene_calls == 0);
}

TEST_CASE("scene panel exposes focus semantics image bounds and pointer capture") {
    using namespace woki;
    ui::Runtime runtime;
    runtime.SetContent(studio::ScenePanel("resident").Size(320, 180));
    runtime.Prepare({.viewport = {320, 180}});

    const auto bounds = runtime.Bounds(studio::kSceneContent);
    REQUIRE(bounds);
    CHECK(bounds->width == 320);
    CHECK(bounds->height == 180);
    CHECK(runtime.Visible(studio::kSceneContent));
    const auto semantic = std::ranges::find(runtime.SemanticsSnapshot(), studio::kSceneContent, &ui::SemanticNode::key);
    REQUIRE(semantic != runtime.SemanticsSnapshot().end());
    CHECK(semantic->semantics.label == "Animated scene viewport");
    CHECK(semantic->semantics.focusable);

    CHECK_FALSE(runtime
            .HandleEvent(ui::PointerEvent{.type = ui::PointerEvent::Type::Down, .position = {20, 20}, .pointer = 7}));
    CHECK(runtime.Focused(studio::kSceneContent));
    CHECK(runtime.Root()->Children().front()->Pressed());
    runtime.CancelCapture();
    CHECK_FALSE(runtime.Root()->Children().front()->Pressed());

    const auto image = std::ranges::find_if(runtime.Display().Operations(), [](const ui::DisplayOp& operation) {
        return std::holds_alternative<ui::ImageOp>(operation);
    });
    REQUIRE(image != runtime.Display().Operations().end());
    CHECK(std::get<ui::ImageOp>(*image).image == studio::kSceneImage);
}

TEST_CASE("Fusion controls stay scoped to their registered panel factory") {
    using namespace woki;
    studio::PanelRegistry panels;
    const ui::Key fusion = ui::Key::From("studio.panel.fusion-controls");
    const ui::Key animation = ui::Key::From("studio.panel.animation");
    int fusion_builds = 0;
    int animation_builds = 0;
    panels.Register(fusion, "Fusion Controls", [&] {
        ++fusion_builds;
        return ui::Text("fusion-only");
    });
    panels.Register(animation, "Animation", [&] {
        ++animation_builds;
        return ui::Text("animation-only");
    });

    const ui::View view = panels.Build(fusion);
    CHECK(view.Content() == "fusion-only");
    CHECK(panels.Title(fusion) == "Fusion Controls");
    CHECK(fusion_builds == 1);
    CHECK(animation_builds == 0);
    CHECK(panels.Build(ui::Key{999}).Content() == "Missing panel");
}

TEST_CASE("text and modal focus isolate scene animation shortcuts") {
    using namespace woki;
    ui::Edit edit;
    ui::Runtime runtime;
    runtime.SetContent(ui::Input(edit, "Name").Id(ui::Key{1}).Size(100, 30));
    runtime.Prepare({.viewport = {100, 30}});
    runtime.HandleEvent(ui::PointerEvent{.type = ui::PointerEvent::Type::Down, .position = {5, 5}});
    REQUIRE(runtime.HasTextOrModalFocus());

    studio::PlatformUiAdapter adapter;
    const auto shortcut = adapter.Convert(events::KeyPressedEvent{events::KeyCode::kSpace});
    REQUIRE(shortcut);
    const bool ui_handled = runtime.HandleEvent(*shortcut);
    const bool route_to_scene = !ui_handled && !runtime.HasTextOrModalFocus();
    CHECK_FALSE(route_to_scene);
}

TEST_CASE("dock workspace retries layout after a usable resize and persists topology") {
    using namespace woki;
    studio::PanelRegistry panels;
    panels.Register(studio::kScenePanel, "Scene", [] { return studio::ScenePanel("loading"); });
    panels.Register(ui::Key::From("studio.panel.inspector"), "Inspector", [] { return ui::Text("inspector"); });
    panels.Register(ui::Key::From("studio.panel.assets"), "Assets", [] { return ui::Text("assets"); });
    panels.Register(ui::Key::From("studio.panel.diagnostics"), "Diagnostics", [] { return ui::Text("diagnostics"); });
    studio::DockWorkspace workspace;
    ui::Runtime runtime;

    runtime.SetContent(workspace.Build(panels, {0, 0, 0, 0}));
    runtime.Prepare({.viewport = {0, 0}});
    CHECK_FALSE(runtime.Visible(studio::kSceneContent));

    runtime.SetContent(workspace.Build(panels, {0, 0, 900, 600}));
    runtime.Prepare({.viewport = {900, 600}});
    const auto resized = runtime.Bounds(studio::kSceneContent);
    REQUIRE(resized);
    CHECK(resized->width > 0);
    CHECK(resized->height > 0);
    CHECK(workspace.PanelVisible(studio::kScenePanel));

    const std::string saved = workspace.Serialize();
    studio::DockWorkspace restored(saved);
    CHECK(restored.Serialize() == saved);
    CHECK(restored.PanelVisible(studio::kScenePanel));
}

TEST_CASE("offscreen resize flags invalidate temporal history and CanvasFrame has value ownership") {
    using namespace woki;
    gfx::RenderView view;
    CHECK_FALSE(view.HistoryInvalid());
    view.flags = gfx::ViewFlags::Resized;
    CHECK(view.HistoryInvalid());
    view.flags = gfx::ViewFlags::CameraCut | gfx::ViewFlags::PipelineChanged;
    CHECK(view.HistoryInvalid());

    static_assert(std::is_copy_constructible_v<gfx::CanvasFrame>);
    gfx::CanvasFrame source{.surface = gfx::SurfaceHandle::Create(1, 1), .width = 32, .height = 24};
    source.vertices.push_back({.x = 4, .y = 5});
    source.indices.push_back(0);
    gfx::CanvasFrame owned = source;
    source.vertices.front().x = 99;
    source.vertices.clear();
    CHECK(owned.vertices.size() == 1);
    CHECK(owned.vertices.front().x == 4);
}

TEST_CASE("headless NullRHI editor frame renders climbing scene then composes canvas") {
    using namespace woki;
    auto instance = rhi::CreateNullInstance();
    REQUIRE(instance);
    auto physical = (*instance)->RequestAdapter();
    REQUIRE(physical);
    auto device = (*physical)->CreateDevice({.label = "headless Studio editor"});
    REQUIRE(device);

    auto target = (*device)->CreateTexture(
        {.size = {640, 360, 1},
            .format = rhi::TextureFormat::RGBA8Unorm,
            .usage = rhi::TextureUsage::RenderAttachment | rhi::TextureUsage::TextureBinding,
            .label = "climbing logical scene offscreen"}
    );
    REQUIRE(target);
    auto view = (*target)->CreateView({.label = "climbing logical scene view"});
    REQUIRE(view);

    ui::Runtime logical_ui;
    logical_ui.SetContent(studio::ScenePanel("climbing.fbx: animated / resident").Size(640, 360));
    logical_ui.Prepare({.viewport = {640, 360}});
    SceneImages images;
    ui::render::BuiltinFontProvider fonts;
    ui::render::Adapter canvas_adapter({images, fonts});
    gfx::CanvasFrame canvas = canvas_adapter.Convert(logical_ui.Display(), gfx::SurfaceHandle::Create(0, 1), 640, 360);
    REQUIRE(canvas.Valid());
    REQUIRE(std::ranges::any_of(canvas.batches, [](const gfx::CanvasBatch& batch) {
        return batch.primitive == gfx::CanvasPrimitive::Image;
    }));

    auto encoder = (*device)->CreateCommandEncoder({.label = "Studio editor frame"});
    REQUIRE(encoder);
    const rhi::RenderPassColorAttachmentDesc attachment{.view = view.get(),
        .load_op = rhi::LoadOp::Clear,
        .store_op = rhi::StoreOp::Store};
    auto scene_pass = (*encoder)->BeginRenderPass(
        rhi::RenderPassDescTyped{.label = "climbing logical scene", .color_attachments = std::span{&attachment, 1}}
    );
    REQUIRE(scene_pass);
    (*scene_pass)->InsertDebugMarker("climbing.logical-scene");
    (*scene_pass)->DrawIndexed(36);
    (*scene_pass)->End();

    auto canvas_pass = (*encoder)->BeginRenderPass(
        rhi::RenderPassDescTyped{.label = "canvas composition and presentation",
            .color_attachments = std::span{&attachment, 1}}
    );
    REQUIRE(canvas_pass);
    (*canvas_pass)->InsertDebugMarker("canvas.compose.present");
    for (const gfx::CanvasBatch& batch : canvas.batches)
        (*canvas_pass)->DrawIndexed(batch.index_count, 1, batch.first_index);
    (*canvas_pass)->End();

    auto commands = (*encoder)->Finish({.label = "Studio editor frame commands"});
    REQUIRE(commands);
    const std::array<rhi::CommandBuffer*, 1> submission{commands->get()};
    REQUIRE((*device)->GetQueue().Submit(submission));

    const auto log = rhi::NullCommandLog(**device);
    CHECK(std::ranges::count(log, "render.begin") == 2);
    CHECK(std::ranges::count(log, "render.end") == 2);
    CHECK(Contains(log, "marker climbing.logical-scene"));
    CHECK(Contains(log, "render.draw_indexed 36 1"));
    CHECK(Contains(log, "marker canvas.compose.present"));
    CHECK(Contains(log, "queue.submit 1"));
}
