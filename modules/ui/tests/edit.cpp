#include <catch2/catch_test_macros.hpp>

#include <woki/ui.hpp>

using namespace woki::ui;

TEST_CASE("editing respects UTF-8 code point boundaries") {
    Edit edit{"a€b"};
    edit.Move(-1);
    edit.Backspace();
    CHECK(edit.Value() == "ab");
    CHECK(edit.Cursor() == 1);
}

TEST_CASE("selection replacement is deterministic") {
    Edit edit{"hello"};
    edit.Select(1, 4);
    edit.Insert("i");
    CHECK(edit.Value() == "hio");
    CHECK_FALSE(edit.Selected());
}
