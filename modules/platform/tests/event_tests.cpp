#include <string>
#include <catch2/catch_test_macros.hpp>

#include <woki/platform.hpp>

TEST_CASE("Event dispatcher invokes matching handlers once") {
    woki::events::WindowResizeEvent event(1280, 720);
    woki::events::EventDispatcher dispatcher(event);

    woki::u32 dispatch_count = 0;
    const bool dispatched = dispatcher.Dispatch<woki::events::WindowResizeEvent>([&](const auto& resize) {
        ++dispatch_count;
        CHECK(resize.width == 1280);
        CHECK(resize.height == 720);
        return true;
    });

    CHECK(dispatched);
    CHECK(event.handled);
    CHECK(dispatch_count == 1);
    CHECK_FALSE(dispatcher.Dispatch<woki::events::WindowResizeEvent>([](const auto&) { return true; }));
}

TEST_CASE("Event formatting preserves type and payload") {
    const woki::events::WindowResizeEvent event(1920, 1080);

    CHECK(woki::events::ToString(event.GetEventType()) == "WindowResized");
    CHECK(woki::events::ToString(event) == "WindowResized [category=Window, handled=false] (1920x1080)");
    CHECK(woki::events::ToJson(event) == R"({"type":"WindowResized","width":1920,"height":1080})");
}
