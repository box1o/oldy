#include <string_view>
#include <catch2/catch_test_macros.hpp>

#include <woki/platform.hpp>

namespace {

struct MisreportedResizeEvent final : woki::events::Event {
    [[nodiscard]] woki::events::EventType GetEventType() const noexcept override {
        return woki::events::EventType::kWindowResized;
    }

    [[nodiscard]] woki::u16 GetCategoryFlags() const noexcept override {
        return static_cast<woki::u16>(woki::events::EventCategory::kWindow);
    }

    [[nodiscard]] std::string_view GetName() const noexcept override {
        return "MisreportedResize";
    }
};

} // namespace

TEST_CASE("Event dispatcher ignores mismatched event types") {
    woki::events::WindowResizeEvent event(1280, 720);
    woki::events::EventDispatcher dispatcher(event);

    woki::u32 dispatch_count = 0;
    const bool dispatched = dispatcher.Dispatch<woki::events::WindowCloseEvent>([&](const auto&) {
        ++dispatch_count;
        return true;
    });

    CHECK_FALSE(dispatched);
    CHECK_FALSE(event.handled);
    CHECK(dispatch_count == 0);
}

TEST_CASE("Event dispatcher preserves false callback results") {
    woki::events::WindowResizeEvent event(1280, 720);
    woki::events::EventDispatcher dispatcher(event);

    CHECK(dispatcher.Dispatch<woki::events::WindowResizeEvent>([](const auto&) { return false; }));
    CHECK_FALSE(event.handled);

    CHECK(dispatcher.Dispatch<woki::events::WindowResizeEvent>([](const auto&) { return true; }));
    CHECK(event.handled);
    CHECK_FALSE(dispatcher.Dispatch<woki::events::WindowResizeEvent>([](const auto&) { return true; }));
}

TEST_CASE("Event dispatcher supports void callbacks without handling the event") {
    woki::events::WindowResizeEvent event(1280, 720);
    woki::events::EventDispatcher dispatcher(event);
    woki::u32 dispatch_count = 0;

    CHECK(dispatcher.Dispatch<woki::events::WindowResizeEvent>([&](const auto& resize) {
        ++dispatch_count;
        CHECK(resize.width == 1280);
        CHECK(resize.height == 720);
    }));
    CHECK_FALSE(event.handled);

    CHECK(dispatcher.Dispatch<woki::events::WindowResizeEvent>([&](const auto&) { ++dispatch_count; }));
    CHECK(dispatch_count == 2);
}

TEST_CASE("Event dispatcher ignores non-bool callback results") {
    woki::events::WindowResizeEvent event(1280, 720);
    woki::events::EventDispatcher dispatcher(event);

    CHECK(dispatcher.Dispatch<woki::events::WindowResizeEvent>([](const auto&) { return 42; }));
    CHECK_FALSE(event.handled);
}

TEST_CASE("Event dispatcher rejects an event whose dynamic type does not match its tag") {
    MisreportedResizeEvent event;
    woki::events::EventDispatcher dispatcher(event);
    bool invoked = false;

    CHECK_FALSE(dispatcher.Dispatch<woki::events::WindowResizeEvent>([&](const auto&) {
        invoked = true;
        return true;
    }));
    CHECK_FALSE(invoked);
    CHECK_FALSE(event.handled);
    CHECK(woki::events::ToString(event) == "MisreportedResize [category=Window, handled=false]");
    CHECK(woki::events::ToJson(event) == R"({"type":"WindowResized"})");
}

TEST_CASE("Events expose representative type name and category metadata") {
    const woki::events::WindowResizeEvent window_event(1920, 1080);
    CHECK(window_event.GetEventType() == woki::events::EventType::kWindowResized);
    CHECK(window_event.GetName() == "WindowResized");
    CHECK(window_event.IsInCategory(woki::events::EventCategory::kWindow));
    CHECK_FALSE(window_event.IsInCategory(woki::events::EventCategory::kInput));

    const woki::events::KeyPressedEvent input_event(woki::events::KeyCode::kA, 2);
    CHECK(input_event.GetEventType() == woki::events::EventType::kKeyPressed);
    CHECK(input_event.GetName() == "KeyPressed");
    CHECK(input_event.IsInCategory(woki::events::EventCategory::kKeyboard));
    CHECK(input_event.IsInCategory(woki::events::EventCategory::kInput));
    CHECK_FALSE(input_event.IsInCategory(woki::events::EventCategory::kMouse));
    CHECK(woki::events::CategoryFlagsToString(input_event.GetCategoryFlags()) == "Input|Keyboard");

    const woki::events::FrameBeginEvent render_event(0.25f);
    CHECK(render_event.GetEventType() == woki::events::EventType::kFrameBegin);
    CHECK(render_event.GetName() == "FrameBegin");
    CHECK(render_event.IsInCategory(woki::events::EventCategory::kRender));

    const woki::events::AppShutdownEvent application_event;
    CHECK(application_event.GetEventType() == woki::events::EventType::kAppShutdown);
    CHECK(application_event.GetName() == "AppShutdown");
    CHECK(application_event.IsInCategory(woki::events::EventCategory::kApplication));
}

TEST_CASE("Extension forwarding policy excludes internal high frequency events") {
    CHECK_FALSE(woki::events::ShouldForwardToExtensions(woki::events::EventType::kAppTick));
    CHECK_FALSE(woki::events::ShouldForwardToExtensions(woki::events::EventType::kAppUpdate));
    CHECK_FALSE(woki::events::ShouldForwardToExtensions(woki::events::EventType::kAppRender));
    CHECK_FALSE(woki::events::ShouldForwardToExtensions(woki::events::EventType::kMouseMoved));
    CHECK_FALSE(woki::events::ShouldForwardToExtensions(woki::events::EventType::kFrameBegin));
    CHECK_FALSE(woki::events::ShouldForwardToExtensions(woki::events::EventType::kFrameEnd));
    CHECK_FALSE(woki::events::ShouldForwardToExtensions(woki::events::EventType::kRenderBegin));
    CHECK_FALSE(woki::events::ShouldForwardToExtensions(woki::events::EventType::kRenderEnd));
    CHECK_FALSE(woki::events::ShouldForwardToExtensions(woki::events::EventType::kSwapBuffers));

    CHECK(woki::events::ShouldForwardToExtensions(woki::events::EventType::kWindowResized));
    CHECK(woki::events::ShouldForwardToExtensions(woki::events::EventType::kKeyPressed));
    CHECK(woki::events::ShouldForwardToExtensions(woki::events::EventType::kAppShutdown));
}

TEST_CASE("Event JSON serialization covers payload variants") {
    CHECK(woki::events::ToJson(woki::events::KeyPressedEvent(woki::events::KeyCode::kA, 3)) == R"({"type":"KeyPressed","key":65,"repeat":3})");
    CHECK(woki::events::ToJson(woki::events::KeyReleasedEvent(woki::events::KeyCode::kEscape)) == R"({"type":"KeyReleased","key":256})");
    CHECK(woki::events::ToJson(woki::events::KeyTypedEvent(0x20AC)) == R"({"type":"KeyTyped","character":8364})");
    CHECK(woki::events::ToJson(woki::events::WindowResizeEvent(1920, 1080)) == R"({"type":"WindowResized","width":1920,"height":1080})");
    CHECK(woki::events::ToJson(woki::events::WindowMovedEvent(-12, 34)) == R"({"type":"WindowMoved","x":-12,"y":34})");
    CHECK(woki::events::ToJson(woki::events::WindowScaleChangedEvent(1.25f, 1.5f)) == R"({"type":"WindowScaleChanged","x":1.250000,"y":1.500000})");
    CHECK(woki::events::ToJson(woki::events::MouseMovedEvent(4.0f, 8.0f, 1.0f, -2.0f)) == R"({"type":"MouseMoved","x":4.000000,"y":8.000000,"deltaX":1.000000,"deltaY":-2.000000})");
    CHECK(woki::events::ToJson(woki::events::MouseScrolledEvent(1.5f, -2.0f)) == R"({"type":"MouseScrolled","offsetX":1.500000,"offsetY":-2.000000})");
    CHECK(woki::events::ToJson(woki::events::MouseButtonPressedEvent(woki::events::MouseButton::kRight, 4.0f, 8.0f)) == R"({"type":"MouseButtonPressed","button":1,"x":4.000000,"y":8.000000})");
    CHECK(woki::events::ToJson(woki::events::MouseButtonReleasedEvent(woki::events::MouseButton::kMiddle, 5.0f, 9.0f)) == R"({"type":"MouseButtonReleased","button":2,"x":5.000000,"y":9.000000})");
    CHECK(woki::events::ToJson(woki::events::MouseButtonClickedEvent(woki::events::MouseButton::kLeft, 6.0f, 10.0f)) == R"({"type":"MouseButtonClicked","button":0,"x":6.000000,"y":10.000000})");
    CHECK(woki::events::ToJson(woki::events::FrameBeginEvent(0.25f)) == R"({"type":"FrameBegin","deltaTime":0.250000})");
    CHECK(woki::events::ToJson(woki::events::ViewportResizeEvent(800, 600)) == R"({"type":"ViewportResized","width":800,"height":600})");
    CHECK(woki::events::ToJson(woki::events::AppTickEvent(0.5f)) == R"({"type":"AppTick","deltaTime":0.500000})");
    CHECK(woki::events::ToJson(woki::events::AppUpdateEvent(0.75f)) == R"({"type":"AppUpdate","deltaTime":0.750000})");
    CHECK(woki::events::ToJson(woki::events::AppShutdownEvent()) == R"({"type":"AppShutdown"})");
}

TEST_CASE("Event formatting preserves type and payload") {
    const woki::events::WindowResizeEvent event(1920, 1080);

    CHECK(woki::events::ToString(event.GetEventType()) == "WindowResized");
    CHECK(woki::events::ToString(event) == "WindowResized [category=Window, handled=false] (1920x1080)");
    CHECK(woki::events::ToJson(event) == R"({"type":"WindowResized","width":1920,"height":1080})");
}
