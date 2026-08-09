#include <vector>
#include <algorithm>
#include <stdexcept>
#include <catch2/catch_test_macros.hpp>

#include <woki/ecs.hpp>

namespace {

struct Position {
    float x = 0.0f;
    float y = 0.0f;

    constexpr Position() noexcept = default;

    explicit constexpr Position(float value) noexcept
        : x(value),
          y(value) {}

    constexpr Position(float x_value, float y_value) noexcept
        : x(x_value),
          y(y_value) {}
};

struct Velocity {
    float x = 0.0f;
    float y = 0.0f;

    constexpr Velocity() noexcept = default;

    constexpr Velocity(float x_value, float y_value) noexcept
        : x(x_value),
          y(y_value) {}
};

struct StableComponent {
    int value{0};

    explicit StableComponent(int initial_value)
        : value(initial_value) {}

    StableComponent(const StableComponent&) = delete;
    StableComponent& operator=(const StableComponent&) = delete;
    StableComponent(StableComponent&&) = delete;
    StableComponent& operator=(StableComponent&&) = delete;
};

struct ThrowingComponent {
    explicit ThrowingComponent(bool should_throw) {
        if (should_throw) {
            throw std::runtime_error("construction failed");
        }
    }
};

struct ConstructionTrackedComponent {
    static inline int constructions = 0;

    int value;

    explicit ConstructionTrackedComponent(int initial_value)
        : value(initial_value) {
        ++constructions;
    }
};

struct LifetimeTrackedComponent {
    static inline int alive = 0;
    static inline int destructions = 0;

    LifetimeTrackedComponent() {
        ++alive;
    }

    ~LifetimeTrackedComponent() {
        --alive;
        ++destructions;
    }

    LifetimeTrackedComponent(const LifetimeTrackedComponent&) = delete;
    LifetimeTrackedComponent& operator=(const LifetimeTrackedComponent&) = delete;
    LifetimeTrackedComponent(LifetimeTrackedComponent&&) = delete;
    LifetimeTrackedComponent& operator=(LifetimeTrackedComponent&&) = delete;
};

} // namespace

TEST_CASE("Registry spawns destroys and recycles entities with new generations") {
    woki::Registry registry;

    const woki::Entity first = registry.Create();
    REQUIRE(registry.Valid(first));
    REQUIRE(registry.Size() == 1);

    REQUIRE(registry.Destroy(first));
    REQUIRE_FALSE(registry.Valid(first));
    REQUIRE(registry.Empty());

    const woki::Entity recycled = registry.Create();
    REQUIRE(registry.Valid(recycled));
    REQUIRE(recycled.Index() == first.Index());
    REQUIRE(recycled.Generation() != first.Generation());
}

TEST_CASE("Registry manages component lifetime and lookup") {
    woki::Registry registry;
    const woki::Entity entity = registry.Create();

    auto& position = registry.Emplace<Position>(entity, 1.0f, 2.0f);
    REQUIRE(position.x == 1.0f);
    REQUIRE(position.y == 2.0f);
    REQUIRE(registry.Has<Position>(entity));
    REQUIRE(registry.Count<Position>() == 1);

    auto& fetched = registry.Get<Position>(entity);
    REQUIRE(&fetched == &position);

    REQUIRE(registry.TryGet<Velocity>(entity) == nullptr);

    auto& velocity = registry.GetOrEmplace<Velocity>(entity, 3.0f, 4.0f);
    REQUIRE(velocity.x == 3.0f);
    REQUIRE(velocity.y == 4.0f);
    REQUIRE(registry.Has<Velocity>(entity));

    REQUIRE(registry.Remove<Position>(entity));
    REQUIRE_FALSE(registry.Has<Position>(entity));
    REQUIRE_FALSE(registry.Remove<Position>(entity));
}

TEST_CASE("Removing a middle component preserves sparse set lookup") {
    woki::Registry registry;
    const woki::Entity first = registry.Create();
    const woki::Entity middle = registry.Create();
    const woki::Entity last = registry.Create();

    registry.Emplace<Position>(first, 1.0f, 10.0f);
    registry.Emplace<Position>(middle, 2.0f, 20.0f);
    registry.Emplace<Position>(last, 3.0f, 30.0f);

    REQUIRE(registry.Remove<Position>(middle));

    REQUIRE(registry.Count<Position>() == 2);
    REQUIRE_FALSE(registry.Has<Position>(middle));
    REQUIRE(registry.TryGet<Position>(middle) == nullptr);
    REQUIRE(registry.Get<Position>(first).x == 1.0f);
    REQUIRE(registry.Get<Position>(first).y == 10.0f);
    REQUIRE(registry.Get<Position>(last).x == 3.0f);
    REQUIRE(registry.Get<Position>(last).y == 30.0f);

    REQUIRE(registry.Remove<Position>(last));
    REQUIRE(registry.Count<Position>() == 1);
    REQUIRE(registry.Get<Position>(first).x == 1.0f);
}

TEST_CASE("Recycled entity indices reject stale component handles") {
    woki::Registry registry;
    const woki::Entity stale = registry.Create();
    registry.Emplace<Position>(stale, 1.0f, 2.0f);

    REQUIRE(registry.Destroy(stale));
    const woki::Entity recycled = registry.Create();

    REQUIRE(recycled.Index() == stale.Index());
    REQUIRE(recycled.Generation() != stale.Generation());
    REQUIRE_FALSE(registry.Valid(stale));
    REQUIRE_FALSE(registry.Has<Position>(stale));
    REQUIRE(registry.TryGet<Position>(stale) == nullptr);
    REQUIRE_FALSE(registry.Remove<Position>(stale));
    REQUIRE_FALSE(registry.Has<Position>(recycled));

    registry.Emplace<Position>(recycled, 3.0f, 4.0f);
    REQUIRE(registry.Get<Position>(recycled).x == 3.0f);
    REQUIRE_FALSE(registry.Remove<Position>(stale));
    REQUIRE(registry.Has<Position>(recycled));
}

TEST_CASE("GetOrEmplace returns an existing component without reconstructing it") {
    ConstructionTrackedComponent::constructions = 0;

    woki::Registry registry;
    const woki::Entity entity = registry.Create();
    ConstructionTrackedComponent& original = registry.GetOrEmplace<ConstructionTrackedComponent>(entity, 42);

    REQUIRE(ConstructionTrackedComponent::constructions == 1);

    ConstructionTrackedComponent& fetched = registry.GetOrEmplace<ConstructionTrackedComponent>(entity, 99);

    REQUIRE(&fetched == &original);
    REQUIRE(fetched.value == 42);
    REQUIRE(ConstructionTrackedComponent::constructions == 1);
    REQUIRE(registry.Count<ConstructionTrackedComponent>() == 1);
}

TEST_CASE("Destroy and clear release component instances exactly once") {
    LifetimeTrackedComponent::alive = 0;
    LifetimeTrackedComponent::destructions = 0;

    woki::Registry registry;
    const woki::Entity first = registry.Create();
    const woki::Entity second = registry.Create();
    registry.Emplace<LifetimeTrackedComponent>(first);
    registry.Emplace<LifetimeTrackedComponent>(second);

    REQUIRE(LifetimeTrackedComponent::alive == 2);
    REQUIRE(registry.Destroy(first));
    REQUIRE(LifetimeTrackedComponent::alive == 1);
    REQUIRE(LifetimeTrackedComponent::destructions == 1);

    registry.Clear<LifetimeTrackedComponent>();
    REQUIRE(LifetimeTrackedComponent::alive == 0);
    REQUIRE(LifetimeTrackedComponent::destructions == 2);
    REQUIRE(registry.Valid(second));

    registry.Emplace<LifetimeTrackedComponent>(second);
    REQUIRE(LifetimeTrackedComponent::alive == 1);

    registry.Clear();
    REQUIRE(LifetimeTrackedComponent::alive == 0);
    REQUIRE(LifetimeTrackedComponent::destructions == 3);
    REQUIRE_FALSE(registry.Valid(second));
}

TEST_CASE("Views iterate entities that match all requested components") {
    woki::Registry registry;
    const woki::Entity a = registry.Create();
    const woki::Entity b = registry.Create();
    const woki::Entity c = registry.Create();

    registry.Emplace<Position>(a, 1.0f, 1.0f);
    registry.Emplace<Velocity>(a, 0.5f, 1.5f);

    registry.Emplace<Position>(b, 2.0f, 2.0f);

    registry.Emplace<Position>(c, 3.0f, 3.0f);
    registry.Emplace<Velocity>(c, -1.0f, 2.0f);

    auto view = registry.View<Position, Velocity>();
    REQUIRE(view.Size() == 2);

    std::vector<woki::Entity> entities;
    for (woki::Entity entity : view) {
        entities.push_back(entity);
    }

    REQUIRE(std::find(entities.begin(), entities.end(), a) != entities.end());
    REQUIRE(std::find(entities.begin(), entities.end(), c) != entities.end());
    REQUIRE(std::find(entities.begin(), entities.end(), b) == entities.end());

    view.Each([](Position& position, Velocity& velocity) {
        position.x += velocity.x;
        position.y += velocity.y;
    });

    REQUIRE(registry.Get<Position>(a).x == 1.5f);
    REQUIRE(registry.Get<Position>(a).y == 2.5f);
    REQUIRE(registry.Get<Position>(c).x == 2.0f);
    REQUIRE(registry.Get<Position>(c).y == 5.0f);
}

TEST_CASE("Const views provide read access to matching entities") {
    woki::Registry registry;
    const woki::Entity entity = registry.Create();
    registry.Emplace<Position>(entity, 4.0f, 8.0f);
    registry.Emplace<Velocity>(entity, 1.0f, 2.0f);

    const woki::Registry& const_registry = registry;
    const auto view = const_registry.View<Position, Velocity>();

    REQUIRE_FALSE(view.Empty());

    const auto tuple = view.Get(entity);
    const auto& position = std::get<0>(tuple);
    const auto& velocity = std::get<1>(tuple);

    REQUIRE(position.x == 4.0f);
    REQUIRE(position.y == 8.0f);
    REQUIRE(velocity.x == 1.0f);
    REQUIRE(velocity.y == 2.0f);
}

TEST_CASE("Registry iterates dense live entities") {
    woki::Registry registry;
    const woki::Entity a = registry.Create();
    const woki::Entity b = registry.Create();
    const woki::Entity c = registry.Create();

    REQUIRE(registry.Destroy(b));

    std::vector<woki::Entity> entities;
    registry.EachEntity([&](woki::Entity entity) { entities.push_back(entity); });

    REQUIRE(entities.size() == 2);
    REQUIRE(std::find(entities.begin(), entities.end(), a) != entities.end());
    REQUIRE(std::find(entities.begin(), entities.end(), c) != entities.end());
    REQUIRE(std::find(entities.begin(), entities.end(), b) == entities.end());
}

TEST_CASE("Registry can query all_of any_of and clear typed storage") {
    woki::Registry registry;
    const woki::Entity entity = registry.Create();

    registry.Emplace<Position>(entity, 10.0f, 20.0f);
    registry.Emplace<Velocity>(entity, 1.0f, 2.0f);

    REQUIRE(registry.AllOf<Position, Velocity>(entity));
    REQUIRE(registry.AnyOf<Position, Velocity>(entity));

    registry.Clear<Velocity>();

    REQUIRE(registry.Has<Position>(entity));
    REQUIRE_FALSE(registry.Has<Velocity>(entity));
    REQUIRE(registry.AllOf<Position>(entity));
    REQUIRE_FALSE(registry.AllOf<Position, Velocity>(entity));
}

TEST_CASE("Clearing a registry permanently invalidates existing handles") {
    woki::Registry registry;
    const woki::Entity old_entity = registry.Create();

    registry.Clear();

    REQUIRE_FALSE(registry.Valid(old_entity));
    REQUIRE(registry.Empty());

    const woki::Entity new_entity = registry.Create();
    REQUIRE(new_entity.Index() == old_entity.Index());
    REQUIRE(new_entity.Generation() != old_entity.Generation());
}

TEST_CASE("Component storage supports immovable components and stable references") {
    woki::Registry registry;
    const woki::Entity first = registry.Create();
    StableComponent& stable = registry.Emplace<StableComponent>(first, 42);

    for (int value = 0; value < 256; ++value) {
        const woki::Entity entity = registry.Create();
        registry.Emplace<StableComponent>(entity, value);
    }

    REQUIRE(&registry.Get<StableComponent>(first) == &stable);
    REQUIRE(stable.value == 42);
    REQUIRE(registry.Remove<StableComponent>(first));
}

TEST_CASE("Failed component construction leaves storage unchanged") {
    woki::Registry registry;
    const woki::Entity entity = registry.Create();

    REQUIRE_THROWS_AS(registry.Emplace<ThrowingComponent>(entity, true), std::runtime_error);
    REQUIRE_FALSE(registry.Has<ThrowingComponent>(entity));
    REQUIRE(registry.Count<ThrowingComponent>() == 0);
}

TEST_CASE("Invalid component reference operations fail safely") {
    woki::Registry registry;
    const woki::Entity entity = registry.Create();
    const woki::Entity dead = woki::Entity::Null();

    REQUIRE_THROWS_AS(registry.Get<Position>(entity), std::out_of_range);
    REQUIRE_THROWS_AS(registry.Emplace<Position>(dead), std::invalid_argument);
    REQUIRE_THROWS_AS(registry.GetOrEmplace<Position>(dead), std::invalid_argument);

    registry.Emplace<Position>(entity);
    REQUIRE_THROWS_AS(registry.Emplace<Position>(entity), std::logic_error);
    REQUIRE(registry.Count<Position>() == 1);
}

TEST_CASE("Views reject entities outside their component intersection") {
    woki::Registry registry;
    const woki::Entity included = registry.Create();
    const woki::Entity excluded = registry.Create();
    registry.Emplace<Position>(included);

    const auto view = registry.View<Position>();

    REQUIRE_THROWS_AS(view.Get(excluded), std::out_of_range);
    REQUIRE_THROWS_AS(view.Get(woki::Entity::Null()), std::out_of_range);
}

TEST_CASE("Views remain valid when registry storage changes") {
    woki::Registry registry;
    const woki::Entity first = registry.Create();
    registry.Emplace<Position>(first, 1.0f, 2.0f);

    const auto view = registry.View<Position>();

    const woki::Entity second = registry.Create();
    registry.Emplace<Position>(second, 3.0f, 4.0f);
    REQUIRE(registry.Destroy(first));

    REQUIRE(view.Empty());
    REQUIRE(registry.View<Position>().Size() == 1);
}

TEST_CASE("View iteration remains valid when callbacks destroy current entities") {
    woki::Registry registry;
    for (int value = 0; value < 3; ++value) {
        const woki::Entity entity = registry.Create();
        registry.Emplace<Position>(entity, static_cast<float>(value));
    }

    std::size_t visited = 0;
    registry.View<Position>().Each([&](woki::Entity entity, Position&) {
        ++visited;
        REQUIRE(registry.Destroy(entity));
    });

    REQUIRE(visited == 3);
    REQUIRE(registry.Empty());
    REQUIRE(registry.Count<Position>() == 0);
}
