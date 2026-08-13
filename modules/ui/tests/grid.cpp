#include <catch2/catch_test_macros.hpp>

#include <woki/ui.hpp>

using namespace woki::ui;

TEST_CASE("grid places spans across deterministic equal tracks") {
    Runtime runtime;
    runtime.SetContent(Grid(3)
            .Size(300, 100)
            .Gap(10)
            .Children(Box().Height(Px{20}).Span(2), Box().Height(Px{30}), Box().Height(Px{10})));
    runtime.Prepare({.viewport = {300, 100}});

    CHECK(runtime.Root()->Children()[0]->Bounds() == Rect{0, 0, 196.66667f, 20});
    CHECK(runtime.Root()->Children()[1]->Bounds().x == 206.66667f);
    CHECK(runtime.Root()->Children()[2]->Bounds().y == 40);
}
