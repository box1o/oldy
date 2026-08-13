#include <algorithm>
#include <array>
#include <limits>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include <woki/core.hpp>

namespace {
struct TestTag;
using TestHandle = woki::Handle<TestTag>;
struct TestVersionTag;
using TestVersion = woki::Version<TestVersionTag>;
} // namespace

TEST_CASE("Version validity ordering and overflow are explicit") {
    constexpr TestVersion invalid;
    constexpr TestVersion one(1);
    static_assert(!invalid.IsValid());
    static_assert(one.IsValid());
    static_assert(one.Value() == 1);
    static_assert(invalid < one);

    TestVersion version(std::numeric_limits<woki::u64>::max() - 1);
    REQUIRE(version.Increment());
    REQUIRE(version.Value() == std::numeric_limits<woki::u64>::max());
    REQUIRE_FALSE(version.Increment());
    REQUIRE(version.Value() == std::numeric_limits<woki::u64>::max());
}

TEST_CASE("SlotMap reuses slots while rejecting stale generations") {
    woki::SlotMap<std::string, TestHandle> slots;
    const auto first = slots.Emplace("first");
    const auto second = slots.Emplace("second");
    REQUIRE(slots.Size() == 2);
    REQUIRE(slots.Get(first) == "first");

    REQUIRE(slots.Remove(first));
    REQUIRE_FALSE(slots.Remove(first));
    REQUIRE_FALSE(slots.Contains(first));
    REQUIRE(slots.TryGet(first) == nullptr);
    REQUIRE_THROWS_AS(slots.Get(first), std::out_of_range);

    const auto replacement = slots.Emplace("replacement");
    REQUIRE(replacement.Index() == first.Index());
    REQUIRE(replacement.Generation() == first.Generation() + 1);
    REQUIRE(slots.Get(second) == "second");
    REQUIRE(slots.Get(replacement) == "replacement");

    std::set<TestHandle> visited;
    slots.ForEach([&](TestHandle handle, std::string& value) {
        visited.insert(handle);
        value += '!';
    });
    REQUIRE((visited == std::set<TestHandle>{second, replacement}));
    REQUIRE(slots.Get(second) == "second!");
    REQUIRE((slots.Handles() == std::vector<TestHandle>{replacement, second}));
}

TEST_CASE("SlotMap properties hold over deterministic mutation sequences") {
    woki::SlotMap<int, TestHandle> slots;
    std::vector<TestHandle> live;
    std::vector<TestHandle> stale;
    for (int step = 0; step < 256; ++step) {
        if (!live.empty() && step % 3 == 0) {
            const auto index = static_cast<std::size_t>(step * 17) % live.size();
            REQUIRE(slots.Remove(live[index]));
            stale.push_back(live[index]);
            live.erase(live.begin() + static_cast<std::ptrdiff_t>(index));
        } else {
            live.push_back(slots.Emplace(step));
        }
        REQUIRE(slots.Size() == live.size());
        for (const auto handle : live)
            REQUIRE(slots.Contains(handle));
        for (const auto handle : stale)
            REQUIRE_FALSE(slots.Contains(handle));
    }
}

TEST_CASE("DenseHandleStorage swap removal repairs sparse indices") {
    woki::DenseHandleStorage<std::string, TestHandle> storage;
    const auto a = TestHandle::Create(7, 1);
    const auto b = TestHandle::Create(2, 4);
    const auto c = TestHandle::Create(20, 9);
    (void)storage.Emplace(a, "a");
    (void)storage.Emplace(b, "b");
    (void)storage.Emplace(c, "c");

    REQUIRE(storage.Remove(b));
    REQUIRE(storage.Size() == 2);
    REQUIRE(storage.DenseIndex(c) == 1);
    REQUIRE(storage.Get(c) == "c");
    REQUIRE(storage.Handles()[1] == c);
    REQUIRE(storage.Values()[1] == "c");
    REQUIRE_FALSE(storage.Contains(TestHandle::Create(c.Index(), c.Generation() + 1)));
    REQUIRE_THROWS_AS(storage.Emplace(c, "duplicate"), std::invalid_argument);
    REQUIRE_THROWS_AS(storage.Emplace(TestHandle{}, "invalid"), std::invalid_argument);
    REQUIRE_FALSE(storage.Remove(b));

    const auto newer_b = TestHandle::Create(b.Index(), b.Generation() + 1);
    (void)storage.Emplace(newer_b, "new b");
    REQUIRE(storage.Contains(newer_b));
    REQUIRE_FALSE(storage.Contains(b));
}

TEST_CASE("DirtyRangeSet merges adjacency clamps bounds and handles arithmetic edges") {
    woki::DirtyRangeSet ranges(100, 0.75F);
    ranges.Mark(20, 10);
    ranges.Mark(0, 5);
    ranges.Mark(5, 15);
    ranges.Mark(90, 100);
    REQUIRE(ranges.Ranges().size() == 2);
    REQUIRE(ranges.Ranges()[0].offset == 0);
    REQUIRE(ranges.Ranges()[0].size == 30);
    REQUIRE(ranges.Ranges()[1].offset == 90);
    REQUIRE(ranges.Ranges()[1].size == 10);
    REQUIRE(ranges.Covered() == 40);

    ranges.Mark(30, 35);
    REQUIRE(ranges.DirtyBytes() == 75);
    REQUIRE(ranges.Whole());
    ranges.Clear();
    REQUIRE(ranges.Empty());

    woki::DirtyRangeSet unbounded;
    unbounded.Mark(std::numeric_limits<woki::u64>::max() - 2, 10);
    REQUIRE(unbounded.Ranges().size() == 1);
    REQUIRE(unbounded.Ranges()[0].size == 2);
}

TEST_CASE("DirtyRangeSet maintains sorted disjoint coverage properties") {
    constexpr woki::u64 size = 128;
    woki::DirtyRangeSet ranges(size, 1.0F);
    std::array<bool, size> expected{};
    for (woki::u64 step = 0; step < 80; ++step) {
        const woki::u64 offset = (step * 37) % 140;
        const woki::u64 count = (step * 11) % 19;
        ranges.Mark(offset, count);
        for (woki::u64 index = offset; index < std::min(size, offset + count); ++index)
            expected[index] = true;

        woki::u64 covered{};
        woki::u64 previous_end{};
        for (const auto& range : ranges.Ranges()) {
            REQUIRE(range.size > 0);
            REQUIRE(range.offset >= previous_end);
            REQUIRE(range.offset + range.size <= size);
            previous_end = range.offset + range.size;
            covered += range.size;
        }
        REQUIRE(covered == static_cast<woki::u64>(std::ranges::count(expected, true)));
        REQUIRE(ranges.Covered() == covered);
    }
}

TEST_CASE("DirtyBitset discards stale bits on resize and coalesces word boundaries") {
    woki::DirtyBitset bits(130);
    bits.Mark(62, 5);
    bits.Mark(129);
    bits.Mark(130);
    REQUIRE(bits.Count() == 6);
    const auto ranges = bits.Ranges(4);
    REQUIRE(ranges.size() == 2);
    REQUIRE(ranges[0].offset == 248);
    REQUIRE(ranges[0].size == 20);
    REQUIRE(ranges[1].offset == 516);
    REQUIRE(ranges[1].size == 4);

    bits.Resize(64);
    REQUIRE(bits.Count() == 0);
    REQUIRE_FALSE(bits.Test(62));
    bits.Mark(0, std::numeric_limits<woki::u32>::max());
    REQUIRE(bits.Count() == 64);
    bits.Clear();
    REQUIRE(bits.Count() == 0);
}
