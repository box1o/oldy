#include <array>
#include <filesystem>

#include <woki/core.hpp>
#include <woki/config.hpp>

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
    const std::array candidates{WebConfigPath(), UserConfigPath(), SystemConfigPath(), DevelopmentConfigPath()};
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

static void ApplyConfig(const woki::config::Document& document, woki::ApplicationSettings& settings) {
    woki::config::ObjectView root(document.Root());
    const auto window = root.Object("window");
    if (!window)
        return;
    if (auto value = window->String("title"))
        settings.title = *value;
    if (auto value = window->Unsigned("width"); value && *value <= std::numeric_limits<woki::u32>::max())
        settings.width = static_cast<woki::u32>(*value);
    if (auto value = window->Unsigned("height"); value && *value <= std::numeric_limits<woki::u32>::max())
        settings.height = static_cast<woki::u32>(*value);
    if (auto value = window->Boolean("floating"))
        settings.floating = *value;
    if (auto value = window->Boolean("fullscreen"))
        settings.fullscreen = *value;
    if (auto value = window->Boolean("resizable"))
        settings.resizable = *value;
    if (auto value = window->Boolean("decorated"))
        settings.decorated = *value;
    if (const auto ui = root.Object("ui"))
        if (auto value = ui->String("dock"))
            settings.dock_layout = *value;
}

static void LoadConfig(const std::filesystem::path& path, woki::ApplicationSettings& settings) {
    if (path.empty())
        return;
    if (!std::filesystem::exists(path)) {
        slog::Warn("Config file '{}' was not found, using defaults", path.string());
        return;
    }
    auto config = woki::config::Document::ParseYamlFile(path);
    if (!config) {
        slog::Error("{}", woki::config::FormatDiagnostics(config.error()));
        return;
    }
    const auto* schema = woki::config::Registry::Global().Find("studio.settings", 1);
    auto diagnostics = woki::config::Validate(*config, *schema);
    woki::config::ObjectView root(config->Root());
    constexpr std::array<std::string_view, 4> root_keys{"$schema", "app", "window", "ui"};
    static_cast<void>(woki::config::RejectUnknown(root, root_keys, diagnostics));
    if (const auto window = root.Object("window")) {
        constexpr std::array<std::string_view, 7>
            window_keys{"title", "width", "height", "floating", "fullscreen", "resizable", "decorated"};
        static_cast<void>(woki::config::RejectUnknown(*window, window_keys, diagnostics));
        const auto wrong = [&](std::string_view key, bool valid) {
            if (const auto* value = window->Find(key); value && !valid)
                diagnostics.push_back(
                    {"STU1001",
                        woki::config::Severity::Error,
                        path.string(),
                        value->Range(),
                        value->Pointer(),
                        "studio setting has the wrong type: " + std::string(key),
                        {}}
                );
        };
        wrong("title", window->String("title").has_value());
        wrong("width", window->Unsigned("width").has_value());
        wrong("height", window->Unsigned("height").has_value());
        for (const auto key : {"floating", "fullscreen", "resizable", "decorated"})
            wrong(key, window->Boolean(key).has_value());
    }
    if (const auto ui = root.Object("ui")) {
        constexpr std::array<std::string_view, 1> ui_keys{"dock"};
        static_cast<void>(woki::config::RejectUnknown(*ui, ui_keys, diagnostics));
        if (const auto* value = ui->Find("dock"); value && !ui->String("dock"))
            diagnostics.push_back(
                {"STU1001",
                    woki::config::Severity::Error,
                    path.string(),
                    value->Range(),
                    value->Pointer(),
                    "studio ui.dock setting must be a string",
                    {}}
            );
    }
    if (!diagnostics.empty()) {
        slog::Error("{}", woki::config::FormatDiagnostics(diagnostics));
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
    settings.user_config_path = UserConfigPath();
    const auto selected = SelectedConfigPath(parser);
    if (parser.Has("config")) {
        LoadConfig(selected, settings);
    } else {
        LoadConfig(SystemConfigPath(), settings);
        LoadConfig(UserConfigPath(), settings);
        if (selected != SystemConfigPath() && selected != UserConfigPath())
            LoadConfig(selected, settings);
    }
    ApplyOverrides(parser, settings);
    Validate(settings);
    return settings;
}

} // namespace studio
