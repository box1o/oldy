#pragma once

#include <array>
#include <filesystem>

#include "woki/core.hpp"

#include "core/application.hpp"

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#endif

namespace studio {

constexpr std::string_view kApplicationName = "studio";
constexpr std::string_view kConfigAppName = "woki";
constexpr std::string_view kConfigFileName = "studio.yaml";

[[nodiscard]] inline auto ConfigFileIn(const std::filesystem::path& directory) -> std::filesystem::path {
    return directory / kConfigFileName;
}

[[nodiscard]] inline auto DevelopmentConfigPath() -> std::filesystem::path {
    return ConfigFileIn(std::filesystem::path("config") / "conf");
}

[[nodiscard]] inline auto UserConfigPath() -> std::filesystem::path {
    auto config_dir = woki::paths::ConfigDirectory(kConfigAppName);
    if (config_dir) {
        return ConfigFileIn(*config_dir);
    }

    return {};
}

[[nodiscard]] inline auto SystemConfigPath() -> std::filesystem::path {
#ifdef WOKI_SYSTEM_CONFIG_DIR
    return ConfigFileIn(WOKI_SYSTEM_CONFIG_DIR);
#else
    return {};
#endif
}

[[nodiscard]] inline auto WebConfigPath() -> std::filesystem::path {
#ifdef __WEB
    return ConfigFileIn(std::filesystem::path("/config") / "conf");
#else
    return {};
#endif
}

[[nodiscard]] inline auto ConfigSearchPaths() {
    return std::array{
        WebConfigPath(),
        DevelopmentConfigPath(),
        UserConfigPath(),
        SystemConfigPath(),
    };
}

[[nodiscard]] inline auto DefaultConfigPath() -> std::filesystem::path {
    for (const auto& path : ConfigSearchPaths()) {
        if (!path.empty() && std::filesystem::exists(path)) {
            return path;
        }
    }

    if (auto user_config = UserConfigPath(); !user_config.empty()) {
        return user_config;
    }

    return DevelopmentConfigPath();
}

[[nodiscard]] inline auto SelectedConfigPath(const woki::ArgumentParser& parser) -> std::filesystem::path {
    if (parser.Has("config")) {
        auto config_path = parser.Get<std::string>("config");
        if (config_path) {
            return std::filesystem::path(*config_path);
        }
    }

    return DefaultConfigPath();
}

inline void AddStudioOptions(woki::ArgumentParser& parser) {
    parser.AddOption<std::string>("config", "Configuration file path");
    parser.AddOption<woki::u32>("width", "Window width override");
    parser.AddOption<woki::u32>("height", "Window height override");
    parser.AddOption<std::string>("title", "Window title override");
    parser.AddFlag("fullscreen", "Run in fullscreen mode");
    parser.AddFlag("floating", "Use a floating window");
    parser.AddFlag("borderless", "Disable window decorations");
}

inline void ApplyWindowConfig(const woki::Config& config, woki::ApplicationSettings& settings) {
    settings.title = config.GetOr<std::string>("window.title", settings.title);
    settings.width = config.GetOr<woki::u32>("window.width", settings.width);
    settings.height = config.GetOr<woki::u32>("window.height", settings.height);
    settings.floating = config.GetOr<bool>("window.floating", settings.floating);
    settings.fullscreen = config.GetOr<bool>("window.fullscreen", settings.fullscreen);
    settings.resizable = config.GetOr<bool>("window.resizable", settings.resizable);
    settings.decorated = config.GetOr<bool>("window.decorated", settings.decorated);
}

inline void ApplyCommandLineOverrides(const woki::ArgumentParser& parser, woki::ApplicationSettings& settings) {
    if (auto width = parser.Get<woki::u32>("width"); width) {
        settings.width = *width;
    }
    if (auto height = parser.Get<woki::u32>("height"); height) {
        settings.height = *height;
    }
    if (auto title = parser.Get<std::string>("title"); title) {
        settings.title = *title;
    }
    if (auto fullscreen = parser.Get<bool>("fullscreen"); fullscreen && *fullscreen) {
        settings.fullscreen = true;
    }
    if (auto floating = parser.Get<bool>("floating"); floating && *floating) {
        settings.floating = true;
    }
    if (auto borderless = parser.Get<bool>("borderless"); borderless && *borderless) {
        settings.decorated = false;
    }
}

inline void ValidateSettings(woki::ApplicationSettings& settings) {
    if (settings.width == 0) {
        slog::Warn("Configured window width was 0, resetting to 1280");
        settings.width = 1280;
    }
    if (settings.height == 0) {
        slog::Warn("Configured window height was 0, resetting to 720");
        settings.height = 720;
    }
    if (settings.title.empty()) {
        settings.title = std::string(kApplicationName);
    }
}

inline void LoadConfig(const std::filesystem::path& path, woki::ApplicationSettings& settings) {
    if (!std::filesystem::exists(path)) {
        slog::Warn("Config file '{}' was not found, using defaults", path.string());
        return;
    }

    auto config = woki::Config::LoadFromYamlFile(path);
    if (!config) {
        config.error().Log();
        return;
    }

    ApplyWindowConfig(*config, settings);
}

inline auto LoadApplicationSettings(int argc, char* argv[]) -> woki::ApplicationSettings {
    woki::ArgumentParser parser(std::string(kApplicationName), "woki studio");
    AddStudioOptions(parser);

    auto parsed = parser.Parse(argc, argv);
    if (!parsed) {
        parsed.error().Log();
        slog::Info("{}", parser.Help());
        return {};
    }

    woki::ApplicationSettings settings;

    LoadConfig(SelectedConfigPath(parser), settings);
    ApplyCommandLineOverrides(parser, settings);
    ValidateSettings(settings);

    return settings;
}

inline auto CreateApplication(int argc, char* argv[]) -> woki::scope<woki::Application> {
    return woki::createScope<woki::Application>(LoadApplicationSettings(argc, argv));
}

inline auto RunApplication(woki::scope<woki::Application> app) -> int {
    if (app == nullptr) {
        return 1;
    }

#ifdef __EMSCRIPTEN__
    struct MainLoopState {
        woki::scope<woki::Application> application;
    };

    auto* state = new MainLoopState{std::move(app)};

    emscripten_set_main_loop_arg(
        [](void* user_data) {
            auto* loop_state = static_cast<MainLoopState*>(user_data);
            if (loop_state == nullptr || loop_state->application == nullptr) {
                emscripten_cancel_main_loop();
                delete loop_state;
                return;
            }

            if (!loop_state->application->Tick()) {
                loop_state->application.reset();
                emscripten_cancel_main_loop();
                delete loop_state;
            }
        },
        state, 0, true
    );

    return 0;
#else
    while (app->Tick()) {
    }
    return 0;
#endif
}

} // namespace studio
