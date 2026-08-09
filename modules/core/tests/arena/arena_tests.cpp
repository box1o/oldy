#include <array>
#include <limits>
#include <string>
#include <vector>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <stdexcept>
#include <memory_resource>
#include <catch2/catch_test_macros.hpp>

#include <woki/core.hpp>

namespace {

struct Counted {
    static inline int alive = 0;
    static inline int destroyed = 0;

    explicit Counted(std::string value)
        : name(std::move(value)) {
        ++alive;
    }

    ~Counted() {
        --alive;
        ++destroyed;
    }

    std::string name;
};

struct Ordered {
    static inline std::vector<int> destroyed;

    explicit Ordered(int value)
        : id(value) {}

    ~Ordered() {
        destroyed.push_back(id);
    }

    int id = 0;
};

struct alignas(64) CacheLineAligned {
    std::array<std::byte, 64> storage{};
};

struct ThrowingConstructor {
    ThrowingConstructor() {
        throw std::runtime_error("construction failed");
    }
};

struct ReentrantThrowingConstructor {
    explicit ReentrantThrowingConstructor(woki::Arena& arena) {
        (void)arena.create<Counted>("nested");
        throw std::runtime_error("construction failed");
    }
};

struct ThrowingDestructor {
    ~ThrowingDestructor() noexcept(false) {
        throw std::runtime_error("destruction failed");
    }
};

template <typename T>
concept ArenaCreatable = requires(woki::Arena& arena) { arena.create<T>(); };

static_assert(!ArenaCreatable<ThrowingDestructor>);

struct ReentrantDestructor {
    explicit ReentrantDestructor(woki::Arena& owner)
        : arena(&owner) {}

    ~ReentrantDestructor() {
        arena->clear();
    }

    woki::Arena* arena;
};

} // namespace

TEST_CASE("Arena starts empty with requested capacity") {
    woki::Arena arena{1024};

    REQUIRE(arena.capacity() == 1024);
    REQUIRE(arena.used() == 0);
    REQUIRE(arena.free() == 1024);
    REQUIRE(arena.peak() == 0);
    REQUIRE(arena.empty());
    REQUIRE(arena.begin() == arena.current());
    REQUIRE(arena.can_fit<std::byte>(1024));
}

TEST_CASE("Arena allocates trivially destructible storage") {
    woki::Arena arena{128};

    auto* values = arena.allocate<int>(4);

    REQUIRE(values != nullptr);
    REQUIRE(arena.contains(values));
    REQUIRE(arena.valid(values));
    REQUIRE(arena.used() >= sizeof(int) * 4);
    REQUIRE(arena.free() == arena.capacity() - arena.used());
    REQUIRE(arena.current() != arena.begin());

    values[0] = 10;
    values[3] = 40;

    REQUIRE(values[0] == 10);
    REQUIRE(values[3] == 40);
}

TEST_CASE("Arena creates objects and destroys them on clear") {
    Counted::alive = 0;
    Counted::destroyed = 0;

    woki::Arena arena{1024};

    auto* first = arena.create<Counted>("first");
    auto* second = arena.create<Counted>("second");

    REQUIRE(first->name == "first");
    REQUIRE(second->name == "second");
    REQUIRE(Counted::alive == 2);
    REQUIRE(Counted::destroyed == 0);
    REQUIRE(arena.valid(first));
    REQUIRE(arena.valid(second));

    arena.clear();

    REQUIRE(Counted::alive == 0);
    REQUIRE(Counted::destroyed == 2);
    REQUIRE(arena.used() == 0);
    REQUIRE(arena.empty());
    REQUIRE(arena.peak() > 0);
}

TEST_CASE("Arena reclaims storage when object construction throws") {
    woki::Arena arena{64};
    const auto used_before = arena.used();

    REQUIRE_THROWS_AS(arena.create<ThrowingConstructor>(), std::runtime_error);
    REQUIRE(arena.used() == used_before);
    REQUIRE(arena.empty());

    auto* bytes = arena.allocate<std::byte>(arena.capacity());
    REQUIRE(bytes != nullptr);
    REQUIRE(arena.used() == arena.capacity());
}

TEST_CASE("Arena does not rewind over reentrant construction") {
    Counted::alive = 0;
    Counted::destroyed = 0;
    woki::Arena arena{256};

    REQUIRE_THROWS_AS(arena.create<ReentrantThrowingConstructor>(arena), std::runtime_error);
    REQUIRE(Counted::alive == 1);
    REQUIRE(arena.used() > 0);

    arena.clear();
    REQUIRE(Counted::alive == 0);
    REQUIRE(Counted::destroyed == 1);
    REQUIRE(arena.empty());
}

TEST_CASE("Arena clear tolerates reentrant clear from a destructor") {
    Counted::alive = 0;
    Counted::destroyed = 0;
    woki::Arena arena{256};
    (void)arena.create<Counted>("first");
    (void)arena.create<ReentrantDestructor>(arena);

    arena.clear();
    REQUIRE(Counted::alive == 0);
    REQUIRE(Counted::destroyed == 1);
    REQUIRE(arena.empty());
}

TEST_CASE("Arena destroys objects in reverse construction order") {
    Ordered::destroyed.clear();

    woki::Arena arena{1024};
    [[maybe_unused]] auto* first = arena.create<Ordered>(1);
    [[maybe_unused]] auto* second = arena.create<Ordered>(2);
    [[maybe_unused]] auto* third = arena.create<Ordered>(3);

    arena.clear();

    REQUIRE(Ordered::destroyed == std::vector<int>{3, 2, 1});
}

TEST_CASE("Arena can be cleared and reused") {
    woki::Arena arena{128};

    auto* first = arena.allocate<int>(4);
    const auto used = arena.used();

    REQUIRE(used > 0);

    arena.clear();

    REQUIRE(arena.used() == 0);
    REQUIRE(arena.empty());

    auto* second = arena.allocate<int>(4);

    REQUIRE(second != nullptr);
    REQUIRE(arena.used() == used);
    REQUIRE(arena.valid(second));
    REQUIRE(arena.peak() >= used);
    REQUIRE(first == second);
}

TEST_CASE("Arena honors over-aligned allocation requests") {
    woki::Arena arena{256};

    auto* object = arena.create<CacheLineAligned>();
    const auto address = reinterpret_cast<std::uintptr_t>(object);

    REQUIRE(address % alignof(CacheLineAligned) == 0);
    REQUIRE(arena.contains(object));
    REQUIRE(arena.valid(object));
}

TEST_CASE("Arena reports capacity checks and throws when exhausted") {
    woki::Arena arena{32};

    REQUIRE(arena.can_fit<std::byte>(32));

    auto* bytes = arena.allocate<std::byte>(32);

    REQUIRE(bytes != nullptr);
    REQUIRE(arena.used() == 32);
    REQUIRE_FALSE(arena.can_fit<std::byte>(1));
    REQUIRE_THROWS_AS(arena.allocate<std::byte>(1), std::bad_alloc);
}

TEST_CASE("Arena rejects invalid or impossible allocation requests") {
    woki::Arena arena{32};

    REQUIRE_FALSE(arena.can_fit(1, 3));
    REQUIRE_THROWS_AS(arena.allocate_bytes(1, 3), std::bad_alloc);
    REQUIRE_THROWS_AS(arena.allocate<std::byte>(std::numeric_limits<woki::u64>::max()), std::bad_alloc);

    woki::Arena empty{0};

    REQUIRE(empty.capacity() == 0);
    REQUIRE_FALSE(empty.contains(empty.begin()));
    REQUIRE_FALSE(empty.valid(empty.begin()));
    REQUIRE_THROWS_AS(empty.allocate<std::byte>(), std::bad_alloc);
}

TEST_CASE("Arena exposes a pmr resource backed by its storage") {
    woki::Arena arena{256};
    std::pmr::polymorphic_allocator<int> allocator{arena.resource()};

    int* value = allocator.allocate(1);
    allocator.construct(value, 42);

    REQUIRE(*value == 42);
    REQUIRE(arena.contains(value));
    REQUIRE(arena.valid(value));

    allocator.destroy(value);
}
