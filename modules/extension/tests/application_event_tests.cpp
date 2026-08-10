#include <array>
#include <limits>
#include <algorithm>
#include <catch2/catch_test_macros.hpp>

#include <woki/ext/plugin.hpp>
#include <woki/ext/application_event.hpp>

namespace {

using woki::ext::ApplicationEventType;

template <typename Payload>
void CheckEmpty(ApplicationEventType type) {
    const auto encoded = woki::ext::EncodeApplicationEvent(Payload{});
    CHECK(encoded.type == type);
    CHECK(encoded.Payload().empty());
    woki_ext_empty_event_t decoded{};
    CHECK(woki_ext_decode_empty_event(encoded.Payload().data(), static_cast<uint32_t>(encoded.Payload().size()), &decoded) == WOKI_EXT_OK);
}

} // namespace

TEST_CASE("Application event IDs and empty payloads cover every exposed event") {
    using enum ApplicationEventType;
    CheckEmpty<woki::ext::WindowClosedPayload>(WindowClosed);
    CheckEmpty<woki::ext::WindowFocusedPayload>(WindowFocused);
    CheckEmpty<woki::ext::WindowLostFocusPayload>(WindowLostFocus);
    CheckEmpty<woki::ext::WindowMinimizedPayload>(WindowMinimized);
    CheckEmpty<woki::ext::WindowMaximizedPayload>(WindowMaximized);
    CheckEmpty<woki::ext::WindowRestoredPayload>(WindowRestored);
    CheckEmpty<woki::ext::MouseEnteredPayload>(MouseEntered);
    CheckEmpty<woki::ext::MouseLeftPayload>(MouseLeft);
    CheckEmpty<woki::ext::AppShutdownPayload>(AppShutdown);
    CheckEmpty<woki::ext::AppSuspendPayload>(AppSuspend);
    CheckEmpty<woki::ext::AppResumePayload>(AppResume);

    CHECK(static_cast<woki::u32>(WindowResized) == WOKI_EXT_EVENT_WINDOW_RESIZED);
    CHECK(static_cast<woki::u32>(WindowMoved) == WOKI_EXT_EVENT_WINDOW_MOVED);
    CHECK(static_cast<woki::u32>(KeyPressed) == WOKI_EXT_EVENT_KEY_PRESSED);
    CHECK(static_cast<woki::u32>(KeyReleased) == WOKI_EXT_EVENT_KEY_RELEASED);
    CHECK(static_cast<woki::u32>(KeyTyped) == WOKI_EXT_EVENT_KEY_TYPED);
    CHECK(static_cast<woki::u32>(MouseScrolled) == WOKI_EXT_EVENT_MOUSE_SCROLLED);
    CHECK(static_cast<woki::u32>(MouseButtonPressed) == WOKI_EXT_EVENT_MOUSE_BUTTON_PRESSED);
    CHECK(static_cast<woki::u32>(MouseButtonReleased) == WOKI_EXT_EVENT_MOUSE_BUTTON_RELEASED);
    CHECK(static_cast<woki::u32>(MouseButtonClicked) == WOKI_EXT_EVENT_MOUSE_BUTTON_CLICKED);
    CHECK(static_cast<woki::u32>(WindowScaleChanged) == WOKI_EXT_EVENT_WINDOW_SCALE_CHANGED);
    CHECK(static_cast<woki::u32>(ViewportResized) == WOKI_EXT_EVENT_VIEWPORT_RESIZED);
}

TEST_CASE("Application event payloads round trip through fixed little endian layouts") {
    using namespace woki::ext;

    const std::array size_events{
        EncodeApplicationEvent(WindowResizedPayload{0x12345678u, 0x90abcdefu}),
        EncodeApplicationEvent(ViewportResizedPayload{0x12345678u, 0x90abcdefu}),
    };
    for (const auto& encoded : size_events) {
        CHECK(std::ranges::equal(encoded.Payload(), std::array<woki::u8, 8>{0x78, 0x56, 0x34, 0x12, 0xef, 0xcd, 0xab, 0x90}));
        woki_ext_size_event_t decoded{};
        const int32_t status = encoded.type == ApplicationEventType::WindowResized ? woki_ext_decode_window_resized_event(encoded.Payload().data(), encoded.size, &decoded)
                                                                                   : woki_ext_decode_viewport_resized_event(encoded.Payload().data(), encoded.size, &decoded);
        REQUIRE(status == WOKI_EXT_OK);
        CHECK(decoded.width == 0x12345678u);
        CHECK(decoded.height == 0x90abcdefu);
    }

    const auto position = EncodeApplicationEvent(WindowMovedPayload{-12, 34});
    woki_ext_position_event_t decoded_position{};
    REQUIRE(woki_ext_decode_position_event(position.Payload().data(), position.size, &decoded_position) == WOKI_EXT_OK);
    CHECK(decoded_position.x == -12);
    CHECK(decoded_position.y == 34);

    const std::array<woki::u8, 8> signed_bits{0x00, 0x00, 0x00, 0x80, 0xff, 0xff, 0xff, 0xff};
    REQUIRE(woki_ext_decode_position_event(signed_bits.data(), static_cast<std::uint32_t>(signed_bits.size()), &decoded_position) == WOKI_EXT_OK);
    CHECK(decoded_position.x == std::numeric_limits<std::int32_t>::min());
    CHECK(decoded_position.y == -1);
    const woki::ext::Event guest_position{WOKI_EXT_EVENT_WINDOW_MOVED, signed_bits.data(), static_cast<woki::u32>(signed_bits.size())};
    const auto guest_decoded = guest_position.Get<woki::ext::WindowMovedEvent>();
    CHECK(guest_decoded.x == std::numeric_limits<woki::i32>::min());
    CHECK(guest_decoded.y == -1);

    const auto pressed = EncodeApplicationEvent(KeyPressedPayload{0x1234, 0x89abcdef});
    CHECK(std::ranges::equal(pressed.Payload(), std::array<woki::u8, 6>{0x34, 0x12, 0xef, 0xcd, 0xab, 0x89}));
    woki_ext_key_pressed_event_t decoded_pressed{};
    REQUIRE(woki_ext_decode_key_pressed_event(pressed.Payload().data(), pressed.size, &decoded_pressed) == WOKI_EXT_OK);
    CHECK(decoded_pressed.key == 0x1234);
    CHECK(decoded_pressed.repeat_count == 0x89abcdef);

    const auto released = EncodeApplicationEvent(KeyReleasedPayload{0x1234});
    woki_ext_key_released_event_t decoded_released{};
    REQUIRE(woki_ext_decode_key_released_event(released.Payload().data(), released.size, &decoded_released) == WOKI_EXT_OK);
    CHECK(decoded_released.key == 0x1234);

    const auto typed = EncodeApplicationEvent(KeyTypedPayload{0x20ac});
    woki_ext_key_typed_event_t decoded_typed{};
    REQUIRE(woki_ext_decode_key_typed_event(typed.Payload().data(), typed.size, &decoded_typed) == WOKI_EXT_OK);
    CHECK(decoded_typed.character == 0x20ac);

    const auto scrolled = EncodeApplicationEvent(MouseScrolledPayload{1.5f, -2.0f});
    woki_ext_mouse_scrolled_event_t decoded_scrolled{};
    REQUIRE(woki_ext_decode_mouse_scrolled_event(scrolled.Payload().data(), scrolled.size, &decoded_scrolled) == WOKI_EXT_OK);
    CHECK(decoded_scrolled.offset_x == 1.5f);
    CHECK(decoded_scrolled.offset_y == -2.0f);

    const std::array button_events{
        EncodeApplicationEvent(MouseButtonPressedPayload{2, 4.0f, -8.0f}),
        EncodeApplicationEvent(MouseButtonReleasedPayload{2, 4.0f, -8.0f}),
        EncodeApplicationEvent(MouseButtonClickedPayload{2, 4.0f, -8.0f}),
    };
    for (const auto& encoded : button_events) {
        woki_ext_mouse_button_event_t decoded{};
        REQUIRE(woki_ext_decode_mouse_button_event(encoded.Payload().data(), encoded.size, &decoded) == WOKI_EXT_OK);
        CHECK(decoded.button == 2);
        CHECK(decoded.x == 4.0f);
        CHECK(decoded.y == -8.0f);
    }

    const auto scale = EncodeApplicationEvent(WindowScaleChangedPayload{1.25f, 1.5f});
    woki_ext_scale_event_t decoded_scale{};
    REQUIRE(woki_ext_decode_scale_event(scale.Payload().data(), scale.size, &decoded_scale) == WOKI_EXT_OK);
    CHECK(decoded_scale.x == 1.25f);
    CHECK(decoded_scale.y == 1.5f);
}

TEST_CASE("Application event decoders reject malformed lengths and pointers") {
    const std::array<uint8_t, 10> bytes{};
    woki_ext_empty_event_t empty{};
    woki_ext_size_event_t size{};
    woki_ext_position_event_t position{};
    woki_ext_scale_event_t scale{};
    woki_ext_key_pressed_event_t pressed{};
    woki_ext_key_released_event_t released{};
    woki_ext_key_typed_event_t typed{};
    woki_ext_mouse_scrolled_event_t scrolled{};
    woki_ext_mouse_button_event_t button{};

    CHECK(woki_ext_decode_empty_event(bytes.data(), 1, &empty) == WOKI_EXT_INVALID);
    CHECK(woki_ext_decode_size_event(bytes.data(), 7, &size) == WOKI_EXT_INVALID);
    CHECK(woki_ext_decode_position_event(bytes.data(), 9, &position) == WOKI_EXT_INVALID);
    CHECK(woki_ext_decode_scale_event(bytes.data(), 7, &scale) == WOKI_EXT_INVALID);
    CHECK(woki_ext_decode_key_pressed_event(bytes.data(), 5, &pressed) == WOKI_EXT_INVALID);
    CHECK(woki_ext_decode_key_released_event(bytes.data(), 3, &released) == WOKI_EXT_INVALID);
    CHECK(woki_ext_decode_key_typed_event(bytes.data(), 3, &typed) == WOKI_EXT_INVALID);
    CHECK(woki_ext_decode_mouse_scrolled_event(bytes.data(), 9, &scrolled) == WOKI_EXT_INVALID);
    CHECK(woki_ext_decode_mouse_button_event(bytes.data(), 8, &button) == WOKI_EXT_INVALID);
    CHECK(woki_ext_decode_size_event(nullptr, 8, &size) == WOKI_EXT_INVALID);
    CHECK(woki_ext_decode_size_event(bytes.data(), 8, nullptr) == WOKI_EXT_INVALID);
}

TEST_CASE("Wildcard and extension event ID helpers are stable") {
    CHECK(woki_ext_event_is_wildcard(WOKI_EXT_EVENT_WILDCARD));
    CHECK_FALSE(woki_ext_event_is_wildcard(WOKI_EXT_EVENT_WINDOW_CLOSED));
    CHECK(woki_ext_extension_event_id(42) == 0x8000002au);
    CHECK(woki::ext::ExtensionEventId(42) == 0x8000002au);
}

TEST_CASE("Application events have stable manifest names") {
    using woki::ext::ApplicationEventType;
    CHECK(woki::ext::ToString(ApplicationEventType::WindowResized) == "window.resized");
    REQUIRE(woki::ext::ParseApplicationEventType("app.resume"));
    CHECK(*woki::ext::ParseApplicationEventType("app.resume") == ApplicationEventType::AppResume);
    CHECK_FALSE(woki::ext::ParseApplicationEventType("unknown"));
}
