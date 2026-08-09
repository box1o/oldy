#include <catch2/catch_test_macros.hpp>

#include <woki/ext/ext.hpp>

TEST_CASE("Extension path safety rejects traversal") {
    REQUIRE_FALSE(woki::ext::HasPathTraversal(std::filesystem::path{"assets/icon.png"}));
    REQUIRE(woki::ext::HasPathTraversal(std::filesystem::path{"../secret"}));
    REQUIRE_FALSE(woki::ext::IsSafeRelativePath(std::filesystem::path{}));
    REQUIRE(woki::ext::IsSafeRelativePath(std::filesystem::path{"assets/icon.png"}));
    REQUIRE_FALSE(woki::ext::IsSafeRelativePath(std::filesystem::path{"/abs"}));
    REQUIRE_FALSE(woki::ext::IsSafeRelativePath(std::filesystem::path{"assets/./icon.png"}));
    REQUIRE_FALSE(woki::ext::IsSafeRelativePath(std::filesystem::path{"..\\secret"}));
    REQUIRE_FALSE(woki::ext::IsSafeRelativePath(std::filesystem::path{"C:/extension.wasm"}));
}
