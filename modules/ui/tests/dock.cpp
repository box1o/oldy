#include <catch2/catch_test_macros.hpp>

#include <woki/ui.hpp>

using namespace woki::ui;

TEST_CASE("dock topology survives validated persistence") {
    Dock dock;
    const auto root = dock.Root().id;
    REQUIRE(dock.Add(root, Key{10}));
    REQUIRE(dock.Split(root, Axis::Horizontal, 0.4f, Key{20}));
    dock.Layout({0, 0, 100, 50});

    auto restored = Dock::Parse(dock.Serialize());
    REQUIRE(restored);
    CHECK(restored->Root().type == DockNode::Type::Split);
    CHECK(restored->Root().ratio == 0.4f);
    CHECK(restored->Root().first->tabs.front() == Key{10});
    CHECK(restored->Root().second->tabs.front() == Key{20});
}

TEST_CASE("dock rejects duplicate node IDs") {
    auto parsed = Dock::Parse(R"({
        "id":1,"type":"split","axis":"horizontal","ratio":0.5,
        "first":{"id":2,"type":"leaf","tabs":[],"active":0},
        "second":{"id":2,"type":"leaf","tabs":[],"active":0}
    })");
    CHECK_FALSE(parsed);
}

TEST_CASE("dock validates persisted schema topology tabs and finite split values") {
    const auto document = [](std::string_view root) {
        return std::string{"{\"$schema\":\"https://schemas.woki.dev/ui.dock/"
                           "v1.schema.json\",\"schema\":1,\"name\":\"ui.dock\",\"root\":"}
               + std::string{root} + "}";
    };

    CHECK_FALSE(Dock::Parse(document(R"({"id":1,"type":"leaf","active":1,"tabs":[10]})")));
    CHECK_FALSE(
        Dock::Parse(document(
            R"({"id":1,"type":"split","axis":"diagonal","ratio":0.5,"first":{"id":2,"type":"leaf","active":0,"tabs":[]},"second":{"id":3,"type":"leaf","active":0,"tabs":[]}})"
        ))
    );
    CHECK_FALSE(
        Dock::Parse(document(
            R"({"id":1,"type":"split","axis":"horizontal","ratio":1.0,"first":{"id":2,"type":"leaf","active":0,"tabs":[]},"second":{"id":3,"type":"leaf","active":0,"tabs":[]}})"
        ))
    );
    CHECK_FALSE(
        Dock::Parse(document(
            R"({"id":1,"type":"split","axis":"horizontal","ratio":0.5,"first":{"id":2,"type":"leaf","active":0,"tabs":[10]},"second":{"id":3,"type":"leaf","active":0,"tabs":[10]}})"
        ))
    );
    CHECK_FALSE(Dock::Parse(R"({"schema":2,"name":"ui.dock","root":{}})"));
}

TEST_CASE("dock split layout enforces minima and partitions both axes") {
    Dock dock;
    const u64 root = dock.Root().id;
    REQUIRE(dock.Add(root, Key{1}));
    REQUIRE(dock.Split(root, Axis::Horizontal, 0.05f, Key{2}));
    dock.Root().min_first = 80;
    dock.Root().min_second = 60;
    dock.Layout({10, 20, 204, 100}, 4);

    CHECK(dock.Root().first->bounds == Rect{10, 20, 80, 100});
    CHECK(dock.Root().second->bounds == Rect{94, 20, 120, 100});

    REQUIRE(dock.SetRatio(root, 0.5f));
    dock.Root().axis = Axis::Vertical;
    dock.Layout({0, 0, 100, 204}, 4);
    CHECK(dock.Root().first->bounds == Rect{0, 0, 100, 100});
    CHECK(dock.Root().second->bounds == Rect{0, 104, 100, 100});
}

TEST_CASE("dock tab activation move drop collapse and persistence preserve invariants") {
    Dock dock;
    const u64 root = dock.Root().id;
    REQUIRE(dock.Add(root, Key{10}));
    REQUIRE(dock.Add(root, Key{11}));
    REQUIRE(dock.Activate(root, 0));
    CHECK(dock.Root().tabs[dock.Root().active] == Key{10});
    REQUIRE(dock.Split(root, Axis::Horizontal, 0.6f, Key{20}));
    const u64 left = dock.Root().first->id;
    const u64 right = dock.Root().second->id;

    REQUIRE(dock.Move(Key{10}, right, 0));
    CHECK(dock.Find(right)->tabs == std::vector<Key>{Key{10}, Key{20}});
    CHECK(dock.Find(right)->active == 0);
    CHECK_FALSE(dock.Move(Key{}, right, 0));
    CHECK_FALSE(dock.SetRatio(left, 0.4f));
    CHECK_FALSE(dock.SetRatio(root, 0.0f));
    CHECK_FALSE(dock.Activate(right, 9));

    REQUIRE(dock.Remove(Key{11}));
    dock.Collapse();
    CHECK(dock.Root().Leaf());
    CHECK(dock.Root().tabs == std::vector<Key>{Key{10}, Key{20}});

    auto restored = Dock::Parse(dock.Serialize());
    REQUIRE(restored);
    CHECK(restored->Serialize() == dock.Serialize());
}
