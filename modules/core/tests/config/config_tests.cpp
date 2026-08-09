#include <random>
#include <fstream>
#include <filesystem>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <woki/core.hpp>

namespace {

std::filesystem::path UniqueTempPath(const std::filesystem::path& directory, std::string_view stem) {
    std::random_device random;
    return directory / (std::string(stem) + "-" + std::to_string(random()) + ".yaml");
}

} // namespace

TEST_CASE("Config stores and retrieves typed values") {
    woki::Config config;
    config.Set("width", "1280");
    config.Set("height", "720");
    config.Set("vsync", "true");
    config.Set("scale", "1.5");

    REQUIRE(config.Has("width"));
    REQUIRE(config.Get<woki::u32>("width").value() == 1280u);
    REQUIRE(config.Get<int>("height").value() == 720);
    REQUIRE(config.Get<bool>("vsync").value());
    REQUIRE(config.Get<float>("scale").value() == Catch::Approx(1.5f));
}

TEST_CASE("Config returns default values when missing") {
    woki::Config config;
    REQUIRE(config.GetOr<int>("missing", 7) == 7);
}

TEST_CASE("Config rejects invalid numeric values") {
    woki::Config config;

    SECTION("integer trailing characters") {
        config.Set("value", "12px");
        const auto value = config.Get<int>("value");
        REQUIRE_FALSE(value.has_value());
        REQUIRE(value.error().Code() == woki::ErrorCode::ParseInvalidFormat);
    }

    SECTION("integer overflow") {
        config.Set("value", "999999999999999999999999");
        const auto value = config.Get<int>("value");
        REQUIRE_FALSE(value.has_value());
        REQUIRE(value.error().Code() == woki::ErrorCode::ParseInvalidFormat);
    }

    SECTION("empty floating point value") {
        config.Set("value", "");
        const auto value = config.Get<float>("value");
        REQUIRE_FALSE(value.has_value());
        REQUIRE(value.error().Code() == woki::ErrorCode::ParseInvalidFormat);
    }

    SECTION("floating point overflow") {
        config.Set("value", "1e9999");
        const auto value = config.Get<double>("value");
        REQUIRE_FALSE(value.has_value());
        REQUIRE(value.error().Code() == woki::ErrorCode::ParseInvalidFormat);
    }
}

TEST_CASE("Build config exposes build mode") {
    REQUIRE((woki::BuildConfig::IsDebug() || woki::BuildConfig::IsRelease()));
}

TEST_CASE("Config loads nested YAML values") {
    const auto temp_dir = woki::paths::TemporaryDirectory();
    REQUIRE(temp_dir.has_value());

    const auto file_path = UniqueTempPath(*temp_dir, "woki_config_test");

    {
        std::ofstream output(file_path);
        REQUIRE(output.good());
        output << "window:\n";
        output << "  width: 1920\n";
        output << "  height: 1080\n";
        output << "  title: test window\n";
    }

    auto config = woki::Config::LoadFromYamlFile(file_path);
    REQUIRE(config.has_value());
    REQUIRE(config->Get<woki::u32>("window.width").value() == 1920u);
    REQUIRE(config->Get<woki::u32>("window.height").value() == 1080u);
    REQUIRE(config->Get<std::string>("window.title").value() == "test window");

    std::filesystem::remove(file_path);
}

TEST_CASE("Config loading is transactional on conversion errors") {
    const auto temp_dir = woki::paths::TemporaryDirectory();
    REQUIRE(temp_dir.has_value());
    const auto file_path = UniqueTempPath(*temp_dir, "woki_config_transaction_test");

    {
        std::ofstream output(file_path);
        REQUIRE(output.good());
        output << "loaded: value\n";
        output << "invalid_sequence:\n";
        output << "  - nested: mapping\n";
    }

    woki::Config config;
    config.Set("existing", "preserved");
    const auto result = config.LoadYaml(file_path);

    REQUIRE_FALSE(result.has_value());
    REQUIRE(config.Get<std::string>("existing").value() == "preserved");
    REQUIRE_FALSE(config.Has("loaded"));
    std::filesystem::remove(file_path);
}
