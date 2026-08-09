#include <random>
#include <fstream>
#include <catch2/catch_test_macros.hpp>

#include <woki/core.hpp>

namespace {

std::filesystem::path UniqueTempPath(const std::filesystem::path& directory, std::string_view stem) {
    std::random_device random;
    return directory / (std::string(stem) + "-" + std::to_string(random()));
}

} // namespace

TEST_CASE("Working and temporary directories resolve") {
    auto working = woki::paths::WorkingDirectory();
    auto temporary = woki::paths::TemporaryDirectory();

    REQUIRE(working.has_value());
    REQUIRE(temporary.has_value());
    REQUIRE_FALSE(working->empty());
    REQUIRE_FALSE(temporary->empty());
}

TEST_CASE("Path join and normalize work") {
    const auto joined = woki::paths::Join("foo", "bar");
    REQUIRE(joined.filename() == "bar");

    auto normalized = woki::paths::Normalize(joined);
    REQUIRE(normalized.has_value());
}

TEST_CASE("App paths resolve for a sample app") {
    auto app_paths = woki::paths::AppPaths("woki-test");
    REQUIRE(app_paths.has_value());
    REQUIRE_FALSE(app_paths->working.empty());
    REQUIRE_FALSE(app_paths->temporary.empty());
    REQUIRE_FALSE(app_paths->logs.empty());
}

TEST_CASE("Logs directory resolves for app name") {
    auto logs = woki::paths::LogsDirectory("woki-test");
    REQUIRE(logs.has_value());
    REQUIRE(logs->filename() == "logs");
}

TEST_CASE("EnsureDirectory rejects an existing regular file") {
    const auto temporary = woki::paths::TemporaryDirectory();
    REQUIRE(temporary.has_value());
    const auto path = UniqueTempPath(*temporary, "woki_core_directory_test_file");
    std::filesystem::remove_all(path);

    {
        std::ofstream output(path);
        REQUIRE(output.good());
    }

    const auto result = woki::paths::EnsureDirectory(path);
    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error().Code() == woki::ErrorCode::FileWriteError);
    std::filesystem::remove(path);
}
