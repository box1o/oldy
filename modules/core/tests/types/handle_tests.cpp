#include <catch2/catch_test_macros.hpp>

#include <unordered_set>

#include <woki/core.hpp>

namespace {
struct TextureTag;
struct BufferTag;
} // namespace

TEST_CASE("Generational handles are invalid by default") {
    const woki::Handle<TextureTag> handle;

    REQUIRE_FALSE(handle.IsValid());
}

TEST_CASE("Generational handles retain identity and type") {
    const auto first = woki::Handle<TextureTag>::Create(7, 3);
    const auto same = woki::Handle<TextureTag>::Create(7, 3);
    const auto newer = woki::Handle<TextureTag>::Create(7, 4);
    const auto other_type = woki::Handle<BufferTag>::Create(7, 3);

    REQUIRE(first.IsValid());
    REQUIRE(first.Index() == 7);
    REQUIRE(first.Generation() == 3);
    REQUIRE(first == same);
    REQUIRE(first != newer);
    REQUIRE(other_type.IsValid());

    std::unordered_set<woki::Handle<TextureTag>> handles{first, newer};
    REQUIRE(handles.contains(same));
    REQUIRE(handles.size() == 2);
}
