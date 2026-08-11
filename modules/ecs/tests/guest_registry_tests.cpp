#include <catch2/catch_test_macros.hpp>

#include <woki/ecs/guest.hpp>

TEST_CASE("Guest ECS uses fixed entity and component storage") {
    struct Position {
        int x;
        int y;
    };

    woki::guest::Registry<2> registry;
    woki::guest::ComponentPool<Position, 2> positions;

    const auto first = registry.Create();
    const auto second = registry.Create();
    CHECK(first);
    CHECK(second);
    CHECK_FALSE(registry.Create());

    auto* position = positions.Emplace(first, Position{3, 4});
    REQUIRE(position != nullptr);
    CHECK(position->x == 3);
    CHECK(positions.Get(first) == position);
    CHECK(positions.Emplace(first, Position{}) == nullptr);

    REQUIRE(registry.Destroy(first));
    const auto reused = registry.Create();
    CHECK(reused.index == first.index);
    CHECK(reused.generation != first.generation);
    CHECK(positions.Get(reused) == nullptr);
    CHECK(positions.Emplace(reused, Position{5, 6}) != nullptr);
}
