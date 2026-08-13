#include <catch2/catch_test_macros.hpp>

#include <woki/events/events.hpp>

#include "layers/extension_event_adapter.hpp"

TEST_CASE("Studio preserves pointer metadata in extension ABI v2") {
    woki::events::PointerData
        pointer{.pointer = 77, .kind = woki::events::PointerKind::kPen, .primary = true, .button = woki::events::PointerButton::kPrimary, .buttons = 1, .x = 12.5f, .y = 24.0f, .pressure = 0.75f, .tilt_x = 15.0f};
    woki::events::PointerDownEvent event(pointer);
    event.metadata = {.timestamp = 2.5, .sequence = 9, .window = 3, .device = 11, .modifiers = 5, .source = woki::events::EventSource::kWeb};
    const auto encoded = woki::EncodeExtensionEvent(event);
    REQUIRE(encoded);
    CHECK(encoded->type == woki::ext::ApplicationEventType::PointerDown);
    woki_ext_pointer_down_event_t decoded{};
    REQUIRE(woki_ext_decode_pointer_down_event(encoded->Payload().data(), static_cast<uint32_t>(encoded->Payload().size()), &decoded) == WOKI_EXT_OK);
    CHECK(decoded.metadata.sequence == 9u);
    CHECK(decoded.metadata.device == 11u);
    CHECK(decoded.metadata.source == 2u);
    CHECK(decoded.pointer_id == 77u);
    CHECK(decoded.kind == static_cast<uint8_t>(woki::events::PointerKind::kPen));
    CHECK(decoded.pressure == 0.75f);
}

TEST_CASE("Studio encodes UTF text and gestures without mouse aliases") {
    woki::events::TextInputEvent text("touch");
    const auto encoded_text = woki::EncodeExtensionEvent(text);
    REQUIRE(encoded_text);
    CHECK(encoded_text->type == woki::ext::ApplicationEventType::TextInput);
    woki_ext_text_input_event_t decoded_text{};
    REQUIRE(woki_ext_decode_text_input_event(encoded_text->Payload().data(), static_cast<uint32_t>(encoded_text->Payload().size()), &decoded_text) == WOKI_EXT_OK);
    CHECK(decoded_text.text_size == 5u);

    woki::events::PinchEvent pinch({.phase = woki::events::GesturePhase::kUpdate, .center_x = 4, .center_y = 5, .pointer_count = 2}, 1.1f, 1.5f);
    const auto encoded_pinch = woki::EncodeExtensionEvent(pinch);
    REQUIRE(encoded_pinch);
    CHECK(encoded_pinch->type == woki::ext::ApplicationEventType::Pinch);
    woki_ext_pinch_event_t decoded_pinch{};
    REQUIRE(woki_ext_decode_pinch_event(encoded_pinch->Payload().data(), static_cast<uint32_t>(encoded_pinch->Payload().size()), &decoded_pinch) == WOKI_EXT_OK);
    CHECK(decoded_pinch.gesture.pointer_count == 2u);
    CHECK(decoded_pinch.scale == 1.5f);
}

TEST_CASE("Studio keeps disconnected controller identity") {
    woki::events::GamepadDisconnectedEvent event(19);
    const auto encoded = woki::EncodeExtensionEvent(event);
    REQUIRE(encoded);
    woki_ext_gamepad_disconnected_event_t decoded{};
    REQUIRE(woki_ext_decode_gamepad_disconnected_event(encoded->Payload().data(), static_cast<uint32_t>(encoded->Payload().size()), &decoded) == WOKI_EXT_OK);
    CHECK(decoded.metadata.device == 19u);
}

TEST_CASE("Studio preserves raw joystick connection snapshots") {
    woki::events::GamepadState state;
    state.device = 23;
    state.connected = true;
    state.name = "raw pad";
    state.guid = "guid";
    state.raw_axes = {0.25f, -0.5f};
    state.raw_buttons = {true, false};
    state.raw_hats = {3};
    woki::events::GamepadConnectedEvent event(std::move(state));
    const auto encoded = woki::EncodeExtensionEvent(event);
    REQUIRE(encoded);
    woki_ext_gamepad_connected_event_t decoded{};
    REQUIRE(woki_ext_decode_gamepad_connected_event(encoded->Payload().data(), static_cast<uint32_t>(encoded->Payload().size()), &decoded) == WOKI_EXT_OK);
    CHECK(decoded.metadata.device == 23u);
    CHECK(decoded.raw_axis_count == 2u);
    CHECK(woki_ext_event_read_f32_le(decoded.raw_axes) == 0.25f);
    CHECK(decoded.raw_button_count == 2u);
    CHECK(decoded.raw_buttons[0] == 1u);
    CHECK(decoded.raw_hat_count == 1u);
    CHECK(decoded.raw_hats[0] == 3u);
}
