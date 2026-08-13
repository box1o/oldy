#include <catch2/catch_test_macros.hpp>

#include <woki/ui.hpp>

using namespace woki::ui;

TEST_CASE("row distributes remaining width to grow children") {
    Runtime runtime;
    runtime.SetContent(Row(Box().Size(20, 10), Box().Width(Grow{}).Height(Px{10})).Gap(5));
    runtime.Prepare({.viewport = {100, 20}});

    CHECK(runtime.Root()->Children()[0]->Bounds().width == 20);
    CHECK(runtime.Root()->Children()[1]->Bounds().x == 25);
    CHECK(runtime.Root()->Children()[1]->Bounds().width == 75);
}

TEST_CASE("absolute anchors stretch between opposing edges") {
    Runtime runtime;
    runtime.SetContent(Stack(Box().Absolute().Left(10).Right(15).Top(5).Bottom(7)));
    runtime.Prepare({.viewport = {100, 80}});

    const Rect rect = runtime.Root()->Children()[0]->Bounds();
    CHECK(rect == Rect{10, 5, 75, 68});
}

TEST_CASE("unchanged layout returns cached measurements") {
    Runtime runtime;
    runtime.SetContent(Column(Text("cached")));
    runtime.Prepare({.viewport = {100, 100}});
    runtime.Prepare({.viewport = {100, 100}});

    CHECK(runtime.Stats().layout.cached > 0);
}

TEST_CASE("grow allocation reserves margins") {
    Runtime runtime;
    runtime.SetContent(Row(Box().Width(Grow{}).Height(Px{10}).Margin(Inset::Axis(5, 0))));
    runtime.Prepare({.viewport = {100, 20}});

    const Rect child = runtime.Root()->Children()[0]->Bounds();
    CHECK(child.x == 5);
    CHECK(child.width == 90);
    CHECK(child.Right() == 95);
}

TEST_CASE("stretch alignment remains inside cross-axis margins") {
    Runtime runtime;
    runtime.SetContent(Row(Box().Width(Px{10}).Margin(Inset::Axis(0, 3))).AlignItems(Align::Stretch));
    runtime.Prepare({.viewport = {100, 20}});

    const Rect child = runtime.Root()->Children()[0]->Bounds();
    CHECK(child.y == 3);
    CHECK(child.height == 14);
    CHECK(child.Bottom() == 17);
}

TEST_CASE("stack honors child size and alignment") {
    Runtime runtime;
    runtime.SetContent(Stack(Box().Size(20, 10)).AlignItems(Align::End).JustifyItems(Justify::Center));
    runtime.Prepare({.viewport = {100, 50}});

    CHECK(runtime.Root()->Children()[0]->Bounds() == Rect{80, 20, 20, 10});
}
