#include <array>
#include <algorithm>
#include <catch2/catch_test_macros.hpp>

#include <woki/core.hpp>

TEST_CASE("ArgumentParser parses flags and options") {
    woki::ArgumentParser parser("woki-test", "core args test");
    parser.AddFlag("verbose", "Enable verbose logging");
    parser.AddOption<int>("width", "Window width");
    parser.AddOption<std::string>("name", "App name", "woki");

    std::array<char, 10> app_name{};
    std::array<char, 10> verbose{};
    std::array<char, 11> width_key{};
    std::array<char, 5> width_value{};
    std::copy_n("app", 4, app_name.data());
    std::copy_n("--verbose", 10, verbose.data());
    std::copy_n("--width", 8, width_key.data());
    std::copy_n("1920", 5, width_value.data());

    char* argv[] = {
        app_name.data(),
        verbose.data(),
        width_key.data(),
        width_value.data(),
    };

    REQUIRE(parser.Parse(4, argv).has_value());
    REQUIRE(parser.Has("verbose"));
    REQUIRE(parser.Get<bool>("verbose").value());
    REQUIRE(parser.Get<int>("width").value() == 1920);
    REQUIRE(parser.Get<std::string>("name").value() == "woki");
}

TEST_CASE("ArgumentParser returns invalid state before parse") {
    woki::ArgumentParser parser;
    auto value = parser.Get<int>("width");
    REQUIRE_FALSE(value.has_value());
    REQUIRE(value.error().Code() == woki::ErrorCode::InvalidState);
}

TEST_CASE("ArgumentParser state reflects the latest parse attempt") {
    woki::ArgumentParser parser;
    parser.AddFlag("verbose", "Enable verbose logging");

    char program[] = "app";
    char verbose[] = "--verbose";
    char* valid_argv[] = {program, verbose};

    REQUIRE(parser.Parse(2, valid_argv).has_value());
    REQUIRE(parser.Parsed());
    REQUIRE(parser.Has("verbose"));

    char unknown[] = "--unknown";
    char* invalid_argv[] = {program, unknown};

    const auto result = parser.Parse(2, invalid_argv);
    REQUIRE_FALSE(result.has_value());
    REQUIRE_FALSE(parser.Parsed());
    REQUIRE_FALSE(parser.Has("verbose"));

    const auto value = parser.Get<bool>("verbose");
    REQUIRE_FALSE(value.has_value());
    REQUIRE(value.error().Code() == woki::ErrorCode::InvalidState);
}
