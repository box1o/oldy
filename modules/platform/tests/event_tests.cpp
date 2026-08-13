#include <algorithm>
#include <catch2/catch_test_macros.hpp>

#include <woki/platform.hpp>

TEST_CASE("dispatcher preserves typed handling semantics") {
    woki::events::PointerData data{.pointer = 7, .kind = woki::events::PointerKind::kTouch, .primary = true, .x = 10, .y = 20};
    woki::events::PointerDownEvent event(data);
    woki::events::EventDispatcher dispatcher(event);
    CHECK_FALSE(dispatcher.Dispatch<woki::events::PointerUpEvent>([](const auto&) { return true; }));
    CHECK(dispatcher.Dispatch<woki::events::PointerDownEvent>([](const auto& value) { return value.pointer_data.pointer == 7; }));
    CHECK(event.handled);
    CHECK_FALSE(dispatcher.Dispatch<woki::events::PointerDownEvent>([](const auto&) { return true; }));
}

TEST_CASE("event metadata and categories are portable") {
    woki::events::PointerMoveEvent event({.pointer = 2, .kind = woki::events::PointerKind::kPen, .pressure = 0.5f});
    event.metadata = {.timestamp = 4.5, .sequence = 9, .window = 3, .device = 8, .modifiers = static_cast<woki::u16>(woki::events::Modifier::kShift), .source = woki::events::EventSource::kSynthetic};
    CHECK(event.IsInCategory(woki::events::EventCategory::kPointer));
    CHECK(event.IsInCategory(woki::events::EventCategory::kInput));
    CHECK(event.Timestamp() == 4.5);
    CHECK(woki::events::ToString(event.GetEventType()) == "PointerMoved");
    CHECK(woki::events::ToJson(event).find("\"sequence\":9") != std::string::npos);
}

TEST_CASE("input state tracks keys pointers and focus cancellation") {
    woki::InputState state;
    woki::events::KeyPressedEvent key(woki::events::KeyCode::kA);
    state.Apply(key);
    CHECK(state.IsKeyDown(woki::events::KeyCode::kA));
    woki::events::PointerDownEvent down({.pointer = 42, .kind = woki::events::PointerKind::kTouch});
    state.Apply(down);
    CHECK(state.Pointers().contains(42));
    woki::events::WindowLostFocusEvent lost;
    state.Apply(lost);
    CHECK_FALSE(state.IsKeyDown(woki::events::KeyCode::kA));
    CHECK(state.Pointers().empty());
}

TEST_CASE("gesture recognizer produces deterministic pan and tap") {
    woki::GestureRecognizer recognizer;
    woki::events::PointerDownEvent down({.pointer = 1, .kind = woki::events::PointerKind::kTouch, .primary = true, .x = 10, .y = 10});
    down.metadata.timestamp = 1;
    CHECK(recognizer.Process(down).empty());
    woki::events::PointerMoveEvent move({.pointer = 1, .kind = woki::events::PointerKind::kTouch, .primary = true, .x = 20, .y = 10, .delta_x = 10});
    move.metadata.timestamp = 1.1;
    auto gestures = recognizer.Process(move);
    REQUIRE(gestures.size() == 1);
    CHECK(gestures.front()->GetEventType() == woki::events::EventType::kPan);
    woki::events::PointerUpEvent up({.pointer = 1, .kind = woki::events::PointerKind::kTouch, .primary = true, .x = 20, .y = 10});
    up.metadata.timestamp = 1.2;
    gestures = recognizer.Process(up);
    REQUIRE(gestures.size() == 1);
    CHECK(static_cast<const woki::events::PanEvent&>(*gestures.front()).gesture.phase == woki::events::GesturePhase::kEnd);
}

TEST_CASE("gesture recognizer produces pinch rotation and long press") {
    woki::GestureRecognizer recognizer;
    woki::events::PointerDownEvent first({.pointer = 1, .kind = woki::events::PointerKind::kTouch, .primary = true, .x = 0, .y = 0});
    first.metadata.timestamp = 2;
    CHECK(recognizer.Process(first).empty());
    auto gestures = recognizer.Update(2.6);
    REQUIRE(gestures.size() == 1);
    CHECK(gestures.front()->GetEventType() == woki::events::EventType::kLongPress);

    recognizer.Reset();
    woki::events::PointerDownEvent a({.pointer = 1, .kind = woki::events::PointerKind::kTouch, .primary = true, .x = 0, .y = 0});
    woki::events::PointerDownEvent b({.pointer = 2, .kind = woki::events::PointerKind::kTouch, .x = 10, .y = 0});
    a.metadata.timestamp = 3;
    b.metadata.timestamp = 3;
    CHECK(recognizer.Process(a).empty());
    CHECK(recognizer.Process(b).empty());
    woki::events::PointerMoveEvent move({.pointer = 2, .kind = woki::events::PointerKind::kTouch, .x = 0, .y = 20, .delta_x = -10, .delta_y = 20});
    move.metadata.timestamp = 3.1;
    gestures = recognizer.Process(move);
    CHECK(std::ranges::any_of(gestures, [](const auto& event) { return event->GetEventType() == woki::events::EventType::kPinch; }));
    CHECK(std::ranges::any_of(gestures, [](const auto& event) { return event->GetEventType() == woki::events::EventType::kRotate; }));
}

TEST_CASE("high frequency internal events stay out of extension forwarding") {
    CHECK_FALSE(woki::events::ShouldForwardToExtensions(woki::events::EventType::kPointerMoved));
    CHECK_FALSE(woki::events::ShouldForwardToExtensions(woki::events::EventType::kFrameBegin));
    CHECK(woki::events::ShouldForwardToExtensions(woki::events::EventType::kPointerDown));
    CHECK(woki::events::ShouldForwardToExtensions(woki::events::EventType::kGamepadConnected));
}
