#include <array>
#include <filesystem>

#include <woki/core.hpp>

#include "settings.hpp"

namespace studio {

constexpr std::string_view kApplicationName = "studio";
constexpr std::string_view kConfigAppName = "woki";
constexpr std::string_view kConfigFileName = "studio.yaml";

static std::filesystem::path ConfigFileIn(const std::filesystem::path& directory) {
    return directory / kConfigFileName;
}

static std::filesystem::path DevelopmentConfigPath() {
    return ConfigFileIn(std::filesystem::path("config") / "conf");
}

static std::filesystem::path UserConfigPath() {
    auto directory = woki::paths::ConfigDirectory(kConfigAppName);
    return directory ? ConfigFileIn(*directory) : std::filesystem::path{};
}

static std::filesystem::path SystemConfigPath() {
#ifdef WOKI_SYSTEM_CONFIG_DIR
    return ConfigFileIn(WOKI_SYSTEM_CONFIG_DIR);
#else
    return {};
#endif
}

static std::filesystem::path WebConfigPath() {
#ifdef __WEB
    return ConfigFileIn(std::filesystem::path("/config") / "conf");
#else
    return {};
#endif
}

static std::filesystem::path DefaultConfigPath() {
    const std::array candidates{WebConfigPath(), DevelopmentConfigPath(), UserConfigPath(), SystemConfigPath()};
    for (const auto& path : candidates) {
        if (!path.empty() && std::filesystem::exists(path))
            return path;
    }
    const auto user = UserConfigPath();
    return user.empty() ? DevelopmentConfigPath() : user;
}

static void AddOptions(woki::ArgumentParser& parser) {
    parser.AddOption<std::string>("config", "Configuration file path");
    parser.AddOption<woki::u32>("width", "Window width override");
    parser.AddOption<woki::u32>("height", "Window height override");
    parser.AddOption<std::string>("title", "Window title override");
    parser.AddFlag("fullscreen", "Run in fullscreen mode");
    parser.AddFlag("floating", "Use a floating window");
    parser.AddFlag("borderless", "Disable window decorations");
}

static std::filesystem::path SelectedConfigPath(const woki::ArgumentParser& parser) {
    if (parser.Has("config")) {
        if (auto path = parser.Get<std::string>("config"))
            return *path;
    }
    return DefaultConfigPath();
}

static void ApplyConfig(const woki::Config& config, woki::ApplicationSettings& settings) {
    settings.title = config.GetOr<std::string>("window.title", settings.title);
    settings.width = config.GetOr<woki::u32>("window.width", settings.width);
    settings.height = config.GetOr<woki::u32>("window.height", settings.height);
    settings.floating = config.GetOr<bool>("window.floating", settings.floating);
    settings.fullscreen = config.GetOr<bool>("window.fullscreen", settings.fullscreen);
    settings.resizable = config.GetOr<bool>("window.resizable", settings.resizable);
    settings.decorated = config.GetOr<bool>("window.decorated", settings.decorated);
}

static void LoadConfig(const std::filesystem::path& path, woki::ApplicationSettings& settings) {
    if (!std::filesystem::exists(path)) {
        slog::Warn("Config file '{}' was not found, using defaults", path.string());
        return;
    }
    auto config = woki::Config::LoadFromYamlFile(path);
    if (!config) {
        config.error().Log();
        return;
    }
    ApplyConfig(*config, settings);
}

static void ApplyOverrides(const woki::ArgumentParser& parser, woki::ApplicationSettings& settings) {
    if (auto width = parser.Get<woki::u32>("width"))
        settings.width = *width;
    if (auto height = parser.Get<woki::u32>("height"))
        settings.height = *height;
    if (auto title = parser.Get<std::string>("title"))
        settings.title = *title;
    if (auto fullscreen = parser.Get<bool>("fullscreen"); fullscreen && *fullscreen)
        settings.fullscreen = true;
    if (auto floating = parser.Get<bool>("floating"); floating && *floating)
        settings.floating = true;
    if (auto borderless = parser.Get<bool>("borderless"); borderless && *borderless)
        settings.decorated = false;
}

static void Validate(woki::ApplicationSettings& settings) {
    if (settings.width == 0) {
        slog::Warn("Configured window width was 0, resetting to 1280");
        settings.width = 1280;
    }
    if (settings.height == 0) {
        slog::Warn("Configured window height was 0, resetting to 720");
        settings.height = 720;
    }
    if (settings.title.empty())
        settings.title = kApplicationName;
}

woki::ApplicationSettings LoadSettings(int argc, char* argv[]) {
    woki::ArgumentParser parser(std::string(kApplicationName), "woki studio");
    AddOptions(parser);
    if (auto parsed = parser.Parse(argc, argv); !parsed) {
        parsed.error().Log();
        slog::Info("{}", parser.Help());
        return {};
    }

    woki::ApplicationSettings settings;
    LoadConfig(SelectedConfigPath(parser), settings);
    ApplyOverrides(parser, settings);
    Validate(settings);
    return settings;
}

} // namespace studio
