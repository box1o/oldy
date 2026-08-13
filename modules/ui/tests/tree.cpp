#include <catch2/catch_test_macros.hpp>

#include <woki/ui.hpp>

using namespace woki::ui;

TEST_CASE("keyed elements survive reorder") {
    Runtime runtime;
    runtime.SetContent(Row(Text("one").Id(Key{1}), Text("two").Id(Key{2})));

    const Element* first = runtime.Root()->Children()[0].get();
    const Element* second = runtime.Root()->Children()[1].get();

    runtime.SetContent(Row(Text("two").Id(Key{2}), Text("one").Id(Key{1})));

    CHECK(runtime.Root()->Children()[0].get() == second);
    CHECK(runtime.Root()->Children()[1].get() == first);
}

TEST_CASE("paint changes do not invalidate layout") {
    Runtime runtime;
    runtime.SetContent(Box().Size(20, 20).Background(Color::rgba(1, 0, 0)));
    runtime.Prepare({.viewport = {100, 100}});

    runtime.SetContent(Box().Size(20, 20).Background(Color::rgba(0, 1, 0)));
    CHECK_FALSE(runtime.Root()->Needs(Dirty::Layout));
    CHECK(runtime.Root()->Needs(Dirty::Paint));
}

TEST_CASE("clean retained subtree reuses its display fragment") {
    Runtime runtime;
    runtime.SetContent(Column(Text("cached"), Box().Size(10, 10).Background(Color::rgba(1, 0, 0))));
    runtime.Prepare({.viewport = {100, 100}});
    const auto operations = runtime.Display().Operations();
    runtime.Prepare({.viewport = {100, 100}});

    CHECK(runtime.Stats().painted == 0);
    CHECK(runtime.Display().Operations() == operations);
}

TEST_CASE("display validation rejects malformed paint values") {
    DisplayList display;
    display.Add(RoundOp{{0, 0, 10, 10}, Radius::All(-1), Color::rgba(1, 1, 1)});
    CHECK_FALSE(display.Valid());

    display.Clear();
    display.Add(ShadowOp{{0, 0, 10, 10}, {}, {.color = Color::rgba(0, 0, 0), .blur = -1}});
    CHECK_FALSE(display.Valid());
}

TEST_CASE("fluent visual styles are emitted with opacity") {
    Runtime runtime;
    runtime.SetContent(
        Box()
            .Size(20, 10)
            .MinSize(10, 5)
            .MaxSize(30, 20)
            .Aspect(2)
            .Background(Color::rgba(1, 0, 0))
            .Border({Color::rgba(0, 1, 0), 1})
            .Shadowed({.color = Color::rgba(0, 0, 0, 0.8f), .blur = 4})
            .Opacity(0.5f)
    );
    runtime.Prepare({.viewport = {30, 20}});

    const auto& operations = runtime.Display().Operations();
    REQUIRE(operations.size() == 3);
    CHECK(std::get<ShadowOp>(operations[0]).shadow.color.a == 0.4f);
    CHECK(std::get<RoundOp>(operations[1]).color.a == 0.5f);
    CHECK(std::get<BorderOp>(operations[2]).stroke.color.a == 0.5f);
}
