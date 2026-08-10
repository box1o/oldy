#include <catch2/catch_test_macros.hpp>

#include <woki/ext/path_safety.hpp>

TEST_CASE("Extension path safety rejects traversal") {
    REQUIRE_FALSE(woki::ext::HasPathTraversal(std::filesystem::path{"assets/icon.png"}));
    REQUIRE(woki::ext::HasPathTraversal(std::filesystem::path{"../secret"}));
    REQUIRE_FALSE(woki::ext::IsSafeRelativePath(std::filesystem::path{}));
    REQUIRE(woki::ext::IsSafeRelativePath(std::filesystem::path{"assets/icon.png"}));
    REQUIRE_FALSE(woki::ext::IsSafeRelativePath(std::filesystem::path{"/abs"}));
    REQUIRE_FALSE(woki::ext::IsSafeRelativePath(std::filesystem::path{"assets/./icon.png"}));
    REQUIRE_FALSE(woki::ext::IsSafeRelativePath(std::filesystem::path{"..\\secret"}));
    REQUIRE_FALSE(woki::ext::IsSafeRelativePath(std::filesystem::path{"C:/extension.wasm"}));
    REQUIRE_FALSE(woki::ext::IsSafeRelativePath(std::filesystem::path{"assets/CON.txt"}));
    REQUIRE_FALSE(woki::ext::IsSafeRelativePath(std::filesystem::path{"assets/icon."}));
    REQUIRE_FALSE(woki::ext::IsSafeRelativePath(std::filesystem::path{"assets/bad?.txt"}));
    REQUIRE_FALSE(woki::ext::IsSafeRelativePath(std::filesystem::path{"assets/bad\x1f.txt"}));
}
