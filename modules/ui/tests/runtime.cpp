#include <catch2/catch_test_macros.hpp>

#include <woki/ui.hpp>

using namespace woki::ui;

TEST_CASE("events follow capture target bubble order") {
    std::vector<int> order;
    auto parent = Box().Size(100, 100).OnEvent([&](EventContext& context, const Event&) {
        order.push_back(context.phase == Phase::Capture ? 1 : 3);
    });
    parent.Add(Box().Size(20, 20).OnEvent([&](EventContext& context, const Event&) {
        if (context.phase == Phase::Target)
            order.push_back(2);
        context.Handle();
    }));

    Runtime runtime;
    runtime.SetContent(std::move(parent));
    runtime.Prepare({.viewport = {100, 100}});
    const bool handled = runtime.HandleEvent(
        PointerEvent{
            .type = PointerEvent::Type::Down,
            .position = {5, 5},
            .button = PointerButton::Primary,
        }
    );

    CHECK(handled);
    CHECK(order == std::vector<int>{1, 2, 3});
}

TEST_CASE("display list remains balanced with clipping") {
    Runtime runtime;
    runtime.SetContent(Box().Size(50, 50).Overflowed(Overflow::Clip).Children(Text("clip")));
    runtime.Prepare({.viewport = {100, 100}});

    CHECK(runtime.Display().Valid());
}

TEST_CASE("pointer captures are independent") {
    Runtime runtime;
    runtime.SetContent(
        Row(Box().Size(20, 20).OnEvent([](EventContext& context, const Event&) { context.Handle(); }),
            Box().Size(20, 20).OnEvent([](EventContext& context, const Event&) { context.Handle(); }))
    );
    runtime.Prepare({.viewport = {40, 20}});

    runtime.HandleEvent(PointerEvent{.type = PointerEvent::Type::Down, .position = {5, 5}, .pointer = 1});
    runtime.HandleEvent(PointerEvent{.type = PointerEvent::Type::Down, .position = {25, 5}, .pointer = 2});

    const auto& children = runtime.Root()->Children();
    CHECK(children[0]->Pressed());
    CHECK(children[1]->Pressed());

    runtime.HandleEvent(PointerEvent{.type = PointerEvent::Type::Up, .position = {100, 100}, .pointer = 1});
    CHECK_FALSE(children[0]->Pressed());
    CHECK(children[1]->Pressed());

    runtime.HandleEvent(PointerEvent{.type = PointerEvent::Type::Cancel, .position = {100, 100}, .pointer = 2});
    CHECK_FALSE(children[1]->Pressed());
}

TEST_CASE("an element stays pressed while another pointer captures it") {
    Runtime runtime;
    runtime.SetContent(Box().Size(20, 20).OnEvent([](EventContext& context, const Event&) { context.Handle(); }));
    runtime.Prepare({.viewport = {20, 20}});

    runtime.HandleEvent(PointerEvent{.type = PointerEvent::Type::Down, .position = {5, 5}, .pointer = 1});
    runtime.HandleEvent(PointerEvent{.type = PointerEvent::Type::Down, .position = {5, 5}, .pointer = 2});
    runtime.HandleEvent(PointerEvent{.type = PointerEvent::Type::Up, .position = {5, 5}, .pointer = 1});

    CHECK(runtime.Root()->Pressed());

    runtime.HandleEvent(PointerEvent{.type = PointerEvent::Type::Up, .position = {5, 5}, .pointer = 2});
    CHECK_FALSE(runtime.Root()->Pressed());
}

TEST_CASE("reused pointer ids and content updates release capture") {
    Runtime runtime;
    runtime.SetContent(
        Row(Box().Size(20, 20).OnEvent([](EventContext& context, const Event&) { context.Handle(); }),
            Box().Size(20, 20).OnEvent([](EventContext& context, const Event&) { context.Handle(); }))
    );
    runtime.Prepare({.viewport = {40, 20}});

    runtime.HandleEvent(PointerEvent{.type = PointerEvent::Type::Down, .position = {5, 5}, .pointer = 7});
    runtime.HandleEvent(PointerEvent{.type = PointerEvent::Type::Down, .position = {25, 5}, .pointer = 7});

    const auto& children = runtime.Root()->Children();
    CHECK_FALSE(children[0]->Pressed());
    CHECK(children[1]->Pressed());

    runtime.SetContent(
        Row(Box().Size(20, 20).OnEvent([](EventContext& context, const Event&) { context.Handle(); }),
            Box().Size(20, 20).OnEvent([](EventContext& context, const Event&) { context.Handle(); }))
    );
    CHECK_FALSE(runtime.Root()->Children()[1]->Pressed());
}

TEST_CASE("visible overflow remains interactive outside parent bounds") {
    int presses = 0;
    Runtime runtime;
    runtime.SetContent(Stack(
        Box().Size(20, 20).Absolute().Top(0).Left(0).Children(
            Box().Size(10, 10).Absolute().Left(30).OnEvent([&](EventContext& context, const Event&) {
                ++presses;
                context.Handle();
            })
        )
    ));
    runtime.Prepare({.viewport = {50, 20}});

    CHECK(runtime.HandleEvent(PointerEvent{.type = PointerEvent::Type::Down, .position = {35, 5}}));
    CHECK(presses == 1);
}

TEST_CASE("clipped overflow blocks interaction outside parent bounds") {
    Runtime runtime;
    runtime.SetContent(Stack(
        Box()
            .Size(20, 20)
            .Absolute()
            .Top(0)
            .Left(0)
            .Overflowed(Overflow::Clip)
            .Children(Box().Size(10, 10).Absolute().Left(30).OnEvent([](EventContext& context, const Event&) {
                context.Handle();
            }))
    ));
    runtime.Prepare({.viewport = {50, 20}});

    CHECK_FALSE(runtime.HandleEvent(PointerEvent{.type = PointerEvent::Type::Down, .position = {35, 5}}));
}
