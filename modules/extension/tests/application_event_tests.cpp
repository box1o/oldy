#include <catch2/catch_test_macros.hpp>

#include <woki/extension/events.hpp>
#include <woki/ext/application_event.hpp>

TEST_CASE("application event ABI v2 IDs match the platform ranges") {
    using enum woki::ext::ApplicationEventType;
    CHECK(WOKI_EXT_EVENT_ABI_VERSION == 2u);
    CHECK(static_cast<woki::u32>(PointerMoved) == 150u);
    CHECK(static_cast<woki::u32>(TextInput) == 160u);
    CHECK(static_cast<woki::u32>(Pinch) == 184u);
    CHECK(static_cast<woki::u32>(GamepadConnected) == 400u);
    CHECK(static_cast<woki::u32>(FilesDropped) == 452u);
}

TEST_CASE("application event v2 metadata is fixed width and endian stable") {
    woki::ext::ApplicationEventEncoder encoder(woki::ext::ApplicationEventType::WindowResized, {.timestamp = 1.5, .sequence = 42, .window = 7, .device = 9, .modifiers = 3, .source = 2, .synthetic = true});
    encoder.U32(1280);
    encoder.U32(720);
    const auto encoded = encoder.Finish();
    REQUIRE(encoded.Payload().size() == 48u);
    woki_ext_window_resized_event_t value{};
    REQUIRE(woki_ext_decode_window_resized_event(encoded.Payload().data(), static_cast<uint32_t>(encoded.Payload().size()), &value) == WOKI_EXT_OK);
    CHECK(value.metadata.schema_version == 2u);
    CHECK(value.metadata.payload_size == 48u);
    CHECK(value.metadata.timestamp == 1.5);
    CHECK(value.metadata.sequence == 42u);
    CHECK(value.metadata.window == 7u);
    CHECK(value.metadata.device == 9u);
    CHECK(value.metadata.modifiers == 3u);
    CHECK(value.width == 1280u);
    CHECK(value.height == 720u);
}

TEST_CASE("variable UTF text payload is decoded without copying") {
    woki::ext::ApplicationEventEncoder encoder(woki::ext::ApplicationEventType::TextInput, {});
    encoder.String("h\xC3\xA9");
    const auto encoded = encoder.Finish();
    woki_ext_text_input_event_t value{};
    REQUIRE(woki_ext_decode_text_input_event(encoded.Payload().data(), static_cast<uint32_t>(encoded.Payload().size()), &value) == WOKI_EXT_OK);
    CHECK(value.text_size == 3u);
    CHECK(value.text[0] == 'h');
    woki::events::Event event{WOKI_EXT_EVENT_TEXT_INPUT, encoded.Payload().data(), static_cast<woki::u32>(encoded.Payload().size())};
    woki::events::EventDispatcher dispatcher(event);
    CHECK(dispatcher.Dispatch<woki::events::TextInputEvent>([](const auto& text) { return text.text_size == 3u; }));
    CHECK(event.Handled());
}

TEST_CASE("malformed v2 payloads are rejected") {
    const woki::u8 invalid[40]{};
    woki_ext_window_closed_event_t value{};
    CHECK(woki_ext_decode_window_closed_event(invalid, 40u, &value) == WOKI_EXT_INVALID);
    CHECK(woki_ext_decode_window_closed_event(nullptr, 0u, &value) == WOKI_EXT_INVALID);
}

TEST_CASE("application event names and extension namespace remain stable") {
    CHECK(woki::ext::ToString(woki::ext::ApplicationEventType::PointerDown) == "pointer_down");
    REQUIRE(woki::ext::ParseApplicationEventType("app.resume"));
    CHECK(*woki::ext::ParseApplicationEventType("app.resume") == woki::ext::ApplicationEventType::AppResume);
    CHECK_FALSE(woki::ext::ParseApplicationEventType("mouse_scrolled"));
    CHECK(woki_ext_event_is_wildcard(WOKI_EXT_EVENT_WILDCARD));
    CHECK(WOKI_EXT_EVENT_EXTENSION_ID(42u) == 0x8000002au);
}
