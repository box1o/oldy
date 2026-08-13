#include <catch2/catch_test_macros.hpp>

#include <woki/ui.hpp>

using namespace woki::ui;

TEST_CASE("button exposes semantics and handles release") {
    int calls = 0;
    Runtime runtime;
    runtime.SetContent(Button("Save", [&] { ++calls; }).Size(80, 34));
    runtime.Prepare({.viewport = {100, 50}});

    CHECK(runtime.Root()->GetSemantics().role == Role::Button);
    runtime.HandleEvent(PointerEvent{.type = PointerEvent::Type::Down, .position = {5, 5}});
    runtime.HandleEvent(PointerEvent{.type = PointerEvent::Type::Up, .position = {5, 5}});
    CHECK(calls == 1);
}

TEST_CASE("focused input accepts renderer-independent text events") {
    Edit edit;
    Runtime runtime;
    runtime.SetContent(Input(edit, "Name").Size(120, 34));
    runtime.Prepare({.viewport = {140, 50}});
    runtime.HandleEvent(PointerEvent{.type = PointerEvent::Type::Down, .position = {5, 5}});
    runtime.HandleEvent(TextEvent{.text = "A"});
    CHECK(edit.Value() == "A");
}

TEST_CASE("widgets accept a custom theme as the final argument") {
    const auto parsed = Theme::Parse(
        R"({"$schema":"https://schemas.woki.dev/ui.theme/v1.schema.json","name":"test","color":{"primary":"#FF0000"}})"
    );
    REQUIRE(parsed);

    Runtime runtime;
    runtime.SetContent(Button("Custom", [] {}, Tone::Primary, ControlSize::Medium, *parsed));
    runtime.Prepare({.viewport = {100, 40}});

    CHECK(runtime.Background(*runtime.Root()).r == 1.0f);
}
