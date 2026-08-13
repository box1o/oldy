#include <catch2/catch_test_macros.hpp>

#include <woki/ui.hpp>

using namespace woki::ui;

TEST_CASE("wheel updates the nearest retained scroll container") {
    Semantics semantics;
    semantics.role = Role::Button;
    semantics.focusable = true;
    Runtime runtime;
    runtime.SetContent(Column(Box().Size(20, 20).SemanticsOf(std::move(semantics)))
            .Size(50, 50)
            .Overflowed(Overflow::Scroll));
    runtime.Prepare({.viewport = {50, 50}});
    runtime.HandleEvent(
        PointerEvent{
            .type = PointerEvent::Type::Wheel,
            .position = {5, 5},
            .delta = {0, 12},
        }
    );

    CHECK(runtime.Root()->ScrollOffset().y == 12);
    CHECK(runtime.Root()->Needs(Dirty::Layout));
}

TEST_CASE("viewport changes invalidate constraint caches") {
    Runtime runtime;
    runtime.SetContent(Box().Width(Percent{1}).Height(Px{10}));
    runtime.Prepare({.viewport = {100, 20}});
    runtime.Prepare({.viewport = {200, 20}});
    CHECK(runtime.Root()->Measured().width == 200);
}
