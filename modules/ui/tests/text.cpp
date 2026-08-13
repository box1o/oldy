#include <catch2/catch_test_macros.hpp>

#include <woki/ui.hpp>

using namespace woki::ui;

TEST_CASE("fallback text measurement wraps within finite constraints") {
    SimpleText text;
    const Size size = text.Measure("abcdefghij", 28, {.size = 10, .line = 1});
    CHECK(size.width == 28);
    CHECK(size.height == 20);
    CHECK(text.Hit("abc", 6, {.size = 10}) == 1);
}
