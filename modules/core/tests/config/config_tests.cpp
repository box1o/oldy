#include <fstream>
#include <filesystem>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <woki/core.hpp>

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

TEST_CASE("Build config exposes build mode") {
    REQUIRE((woki::BuildConfig::IsDebug() || woki::BuildConfig::IsRelease()));
}

TEST_CASE("Config loads nested YAML values") {
    const auto temp_dir = woki::paths::TemporaryDirectory();
    REQUIRE(temp_dir.has_value());

    const auto file_path = *temp_dir / "woki_config_test.yaml";

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
