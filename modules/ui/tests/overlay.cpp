#include <catch2/catch_test_macros.hpp>

#include <woki/ui.hpp>

using namespace woki::ui;

TEST_CASE("dialog surface is centered by absolute layout") {
    Runtime runtime;
    runtime.SetContent(Dialog(Box().Size(100, 40)));
    runtime.Prepare({.viewport = {800, 600}});

    const Element* portal = runtime.Root();
    const Element* stack = portal->Children()[0].get();
    const Rect surface = stack->Children()[1]->Bounds();
    CHECK(surface.x == 160);
    CHECK(surface.y == 268);
    CHECK(surface.width == 480);
}

TEST_CASE("hover style starts a paint-only transition") {
    Runtime runtime;
    runtime.SetContent(
        Box()
            .Id(Key{8})
            .Size(40, 40)
            .Background(Color::rgba(0, 0, 0))
            .Hover(Color::rgba(1, 1, 1))
            .Transitioned({.duration = std::chrono::milliseconds{100}, .curve = Curve::Linear})
    );
    const auto start = std::chrono::steady_clock::time_point{};
    runtime.Prepare({.viewport = {50, 50}, .time = start});
    runtime.HandleEvent(PointerEvent{.type = PointerEvent::Type::Move, .position = {5, 5}});
    CHECK_FALSE(runtime.Root()->Needs(Dirty::Layout));
    CHECK(runtime.Root()->Needs(Dirty::Paint));
    runtime.Prepare({.viewport = {50, 50}, .time = start + std::chrono::milliseconds{50}});

    CHECK(runtime.Root()->Hovered());
    CHECK(runtime.Background(*runtime.Root()).r == 0.5f);
    const auto& operation = std::get<RoundOp>(runtime.Display().Operations().front());
    CHECK(operation.color.r == 0.5f);
}

TEST_CASE("motion tracks use internal element identity") {
    Runtime runtime;
    runtime.SetContent(
        Row(Box().Id(Key{1}).Size(10, 10).Background(Color::rgba(1, 0, 0)),
            Box().Id(Key{1}).Size(10, 10).Background(Color::rgba(0, 0, 1)))
    );
    runtime.Prepare({.viewport = {20, 10}});

    CHECK(runtime.Background(*runtime.Root()->Children()[0]) == Color::rgba(1, 0, 0));
    CHECK(runtime.Background(*runtime.Root()->Children()[1]) == Color::rgba(0, 0, 1));
}

TEST_CASE("visible overflow paints children outside parent bounds") {
    Runtime runtime;
    runtime.SetContent(Stack(
        Box().Size(10, 10).Absolute().Left(-20).Children(
            Box().Size(10, 10).Absolute().Left(25).Background(Color::rgba(1, 0, 0))
        )
    ));
    runtime.Prepare({.viewport = {20, 20}});

    REQUIRE(runtime.Display().Operations().size() == 1);
    CHECK(std::get<RoundOp>(runtime.Display().Operations().front()).rect.x == 5);
}
