#include <algorithm>
#include <string>
#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <woki/ui.hpp>

using namespace woki::ui;
using Catch::Approx;

TEST_CASE("image views paint image operations with fit and opacity") {
    Runtime runtime;
    runtime.SetContent(Box().Id(Key{1}).Size(64, 32).Opacity(0.4f).Image(ImageId{91}, ImageFit::Contain));
    runtime.Prepare({.viewport = {100, 100}});

    REQUIRE(runtime.Display().Operations().size() == 1);
    const auto& image = std::get<ImageOp>(runtime.Display().Operations().front());
    CHECK(image.image == ImageId{91});
    CHECK(image.fit == ImageFit::Contain);
    CHECK(image.rect == Rect{0, 0, 64, 32});
    CHECK(image.tint.a == Approx(0.4f));
}

TEST_CASE("portal overlay is painted last and receives input before covered content") {
    std::vector<std::string> calls;
    Runtime runtime;
    runtime.SetContent(Stack(
        Box().Size(80, 80).Background(Color::rgba(1, 0, 0)).OnEvent([&](EventContext& context, const Event&) {
            calls.emplace_back("base");
            context.Handle();
        }),
        Portal(Box().Size(40, 40).Background(Color::rgba(0, 0, 1)).OnEvent([&](EventContext& context, const Event&) {
            calls.emplace_back("portal");
            context.Handle();
        }))
    ));
    runtime.Prepare({.viewport = {80, 80}});

    REQUIRE(runtime.Display().Operations().size() == 2);
    CHECK(std::get<RoundOp>(runtime.Display().Operations().back()).color == Color::rgba(0, 0, 1));
    CHECK(runtime.HandleEvent(PointerEvent{.type = PointerEvent::Type::Down, .position = {10, 10}}));
    CHECK(calls == std::vector<std::string>{"portal"});
}

TEST_CASE("stopping capture prevents target and bubble while handling is retained") {
    std::vector<Phase> phases;
    Runtime runtime;
    runtime.SetContent(
        Box()
            .Size(50, 50)
            .OnEvent([&](EventContext& context, const Event&) {
                phases.push_back(context.phase);
                if (context.phase == Phase::Capture)
                    context.Stop();
            })
            .Children(Box().Size(20, 20).OnEvent([&](EventContext& context, const Event&) {
                phases.push_back(context.phase);
            }))
    );
    runtime.Prepare({.viewport = {50, 50}});

    CHECK(runtime.HandleEvent(PointerEvent{.type = PointerEvent::Type::Down, .position = {5, 5}}));
    CHECK(phases == std::vector<Phase>{Phase::Capture});
}

TEST_CASE("rich pointer gesture modifier and composition payloads survive routing") {
    std::vector<Event> received;
    Runtime runtime;
    runtime.SetContent(
        Box()
            .Id(Key{7})
            .Size(80, 40)
            .SemanticsOf({.role = Role::Input, .label = "composer", .focusable = true})
            .OnEvent([&](EventContext& context, const Event& event) {
                if (context.phase == Phase::Target) {
                    received.push_back(event);
                    context.Handle();
                }
            })
    );
    runtime.Prepare({.viewport = {80, 40}});
    REQUIRE(runtime.HandleEvent(
        PointerEvent{.type = PointerEvent::Type::Down,
            .position = {4, 5},
            .button = PointerButton::Primary,
            .kind = PointerKind::Pen,
            .buttons = 5,
            .modifiers = static_cast<Modifiers>(Modifier::Shift) | static_cast<Modifiers>(Modifier::Alt),
            .pressure = 0.75f,
            .contact_width = 3,
            .contact_height = 5,
            .tilt_x = 12,
            .tilt_y = -4,
            .pointer = 9}
    ));
    REQUIRE(runtime.HandleEvent(
        CompositionEvent{.type = CompositionEvent::Type::Update,
            .text = "\xE3\x81\x82",
            .selection_start = 1,
            .selection_length = 2}
    ));
    REQUIRE(runtime.HandleEvent(
        GestureEvent{.type = GestureEvent::Type::Pinch,
            .phase = GestureEvent::Phase::Update,
            .center = {10, 12},
            .delta = {2, -1},
            .velocity = {8, 3},
            .scale = 1.25f,
            .pointer_count = 2,
            .modifiers = static_cast<Modifiers>(Modifier::Control)}
    ));

    REQUIRE(received.size() == 3);
    const auto& pointer = std::get<PointerEvent>(received[0]);
    CHECK(pointer.kind == PointerKind::Pen);
    CHECK(pointer.modifiers == 5);
    CHECK(pointer.pressure == Approx(0.75f));
    CHECK(pointer.pointer == 9);
    const auto& composition = std::get<CompositionEvent>(received[1]);
    CHECK(composition.text == "\xE3\x81\x82");
    CHECK(composition.selection_start == 1);
    CHECK(composition.selection_length == 2);
    const auto& gesture = std::get<GestureEvent>(received[2]);
    CHECK(gesture.scale == Approx(1.25f));
    CHECK(gesture.pointer_count == 2);
}

TEST_CASE("runtime reports stable bounds visibility focus and semantic hierarchy") {
    const Key root_key{100};
    const Key first_key{101};
    const Key second_key{102};
    Runtime runtime;
    runtime.SetContent(Column(
        Box().Id(first_key).Size(60, 20).SemanticsOf(
            {.role = Role::Button, .label = "first", .focusable = true, .tab_index = 2}
        ),
        Box()
            .Id(second_key)
            .Size(60, 20)
            .SemanticsOf({.role = Role::Input, .label = "second", .value = "value", .focusable = true, .tab_index = 1})
    )
            .Id(root_key)
            .Size(60, 40));
    runtime.Prepare({.viewport = {60, 30}});

    CHECK(runtime.Bounds(first_key) == Rect{0, 0, 60, 20});
    CHECK(runtime.Visible(first_key));
    CHECK(runtime.Visible(second_key));
    CHECK_FALSE(runtime.Bounds(Key{999}));
    CHECK(runtime.HandleEvent(KeyEvent{.key = KeyCode::Tab, .pressed = true}));
    CHECK(runtime.Focused(second_key));
    CHECK(runtime.HasTextOrModalFocus());

    const auto& semantics = runtime.SemanticsSnapshot();
    const auto second = std::ranges::find(semantics, second_key, &SemanticNode::key);
    REQUIRE(second != semantics.end());
    CHECK(second->parent == root_key);
    CHECK(second->focused);
    CHECK(second->semantics.value == "value");
}

TEST_CASE("theme accepts bounded oklch and rejects malformed or out of range values") {
    const auto theme = Theme::Parse(R"json({
        "$schema":"https://schemas.woki.dev/ui.theme/v1.schema.json",
        "name":"oklch",
        "color":{"accent":"oklch(0.7 0.14 220 / 35%)"}
})json");
    REQUIRE(theme);
    const Color accent = theme->ColorOf("accent");
    CHECK(accent.r >= 0.0f);
    CHECK(accent.r <= 1.0f);
    CHECK(accent.g >= 0.0f);
    CHECK(accent.g <= 1.0f);
    CHECK(accent.b >= 0.0f);
    CHECK(accent.b <= 1.0f);
    CHECK(accent.a == Approx(0.35f));

    CHECK_FALSE(Theme::Parse(R"json({"$schema":"x","name":"bad","color":{"x":"oklch(1.1 0.2 0)"}})json"));
    CHECK_FALSE(Theme::Parse(R"json({"$schema":"x","name":"bad","color":{"x":"oklch(0.5 nope 0)"}})json"));
}
