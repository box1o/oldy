#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <sstream>
#include <optional>
#include <woki/config.hpp>

#include <woki/ui/theme/theme.hpp>

namespace woki::ui {

namespace {

std::expected<Color, std::string> ParseColor(std::string_view value) {
    if (value.starts_with("oklch(") && value.ends_with(')')) {
        std::string components(value.substr(6, value.size() - 7));
        std::ranges::replace(components, '/', ' ');
        std::istringstream input(components);
        f32 lightness{}, chroma{}, hue{}, alpha{1.0F};
        if (!(input >> lightness >> chroma >> hue))
            return std::unexpected("oklch color requires lightness, chroma, and hue");
        std::string alpha_text;
        if (input >> alpha_text) {
            const bool percent = alpha_text.ends_with('%');
            if (percent)
                alpha_text.pop_back();
            const auto parsed = std::from_chars(alpha_text.data(), alpha_text.data() + alpha_text.size(), alpha);
            if (parsed.ec != std::errc{} || parsed.ptr != alpha_text.data() + alpha_text.size())
                return std::unexpected("oklch alpha is invalid");
            if (percent)
                alpha /= 100.0F;
        }
        if (!std::isfinite(lightness) || !std::isfinite(chroma) || !std::isfinite(hue) || !std::isfinite(alpha)
            || lightness < 0 || lightness > 1 || chroma < 0 || alpha < 0 || alpha > 1)
            return std::unexpected("oklch components are outside supported ranges");
        constexpr f32 pi = 3.14159265358979323846F;
        const f32 angle = hue * pi / 180.0F;
        const f32 a = chroma * std::cos(angle);
        const f32 b = chroma * std::sin(angle);
        const f32 l = lightness + 0.3963377774F * a + 0.2158037573F * b;
        const f32 m = lightness - 0.1055613458F * a - 0.0638541728F * b;
        const f32 s = lightness - 0.0894841775F * a - 1.2914855480F * b;
        const f32 l3 = l * l * l, m3 = m * m * m, s3 = s * s * s;
        return Color::rgba(
            std::clamp(4.0767416621F * l3 - 3.3077115913F * m3 + 0.2309699292F * s3, 0.0F, 1.0F),
            std::clamp(-1.2684380046F * l3 + 2.6097574011F * m3 - 0.3413193965F * s3, 0.0F, 1.0F),
            std::clamp(-0.0041960863F * l3 - 0.7034186147F * m3 + 1.7076147010F * s3, 0.0F, 1.0F),
            alpha
        );
    }
    if (value.size() != 7 && value.size() != 9)
        return std::unexpected("color must be #RRGGBB, #RRGGBBAA, or oklch(L C H / A)");
    if (value.front() != '#')
        return std::unexpected("color must begin with #");
    u32 packed{};
    const auto result = std::from_chars(value.data() + 1, value.data() + value.size(), packed, 16);
    if (result.ec != std::errc{} || result.ptr != value.data() + value.size()) {
        return std::unexpected("color contains invalid hexadecimal digits");
    }
    if (value.size() == 7)
        packed = (packed << 8) | 0xFFu;
    constexpr f32 scale = 1.0f / 255.0f;
    return Color::rgba(
        static_cast<f32>((packed >> 24) & 0xFFu) * scale,
        static_cast<f32>((packed >> 16) & 0xFFu) * scale,
        static_cast<f32>((packed >> 8) & 0xFFu) * scale,
        static_cast<f32>(packed & 0xFFu) * scale
    );
}

std::optional<Curve> ParseCurve(std::string_view value) {
    if (value == "linear")
        return Curve::Linear;
    if (value == "in")
        return Curve::In;
    if (value == "in-out")
        return Curve::InOut;
    if (value == "out")
        return Curve::Out;
    return std::nullopt;
}

template <typename T>
T Find(const std::unordered_map<std::string, T>& values, std::string_view name, T fallback) {
    const auto found = values.find(std::string{name});
    return found == values.end() ? std::move(fallback) : found->second;
}

} // namespace

const Theme& Theme::Default() {
    static const Theme theme = [] {
        Theme value;
        value.colors_ = {
            {"background", Color::rgba(0.055f, 0.055f, 0.063f)},
            {"foreground", Color::rgba(0.96f, 0.96f, 0.97f)},
            {"card", Color::rgba(0.075f, 0.075f, 0.086f)},
            {"primary", Color::rgba(0.23f, 0.51f, 0.96f)},
            {"secondary", Color::rgba(0.16f, 0.16f, 0.18f)},
            {"accent", Color::rgba(0.20f, 0.20f, 0.23f)},
            {"pressed", Color::rgba(0.16f, 0.40f, 0.86f)},
            {"destructive", Color::rgba(0.86f, 0.20f, 0.22f)},
            {"muted", Color::rgba(0.22f, 0.22f, 0.25f)},
            {"input", Color::rgba(0.07f, 0.07f, 0.08f)},
            {"border", Color::rgba(0.25f, 0.25f, 0.28f)},
        };
        value.spaces_ = {{"panel", 12.0f}};
        value.radii_ = {{"control", 6.0f}, {"panel", 8.0f}};
        value.types_ = {{"body", {.family = "sans", .size = 14.0f, .line = 1.2f, .weight = 400}}};
        value.motions_ = {{"fast", {.duration = std::chrono::milliseconds{100}, .curve = Curve::Out}}};
        return value;
    }();
    return theme;
}

std::expected<Theme, std::string> Theme::Parse(std::string_view jsonc) {
    auto parsed = config::Json::Parse(jsonc, "ui.theme");
    if (!parsed)
        return std::unexpected(config::FormatDiagnostics(parsed.error()));
    const config::Json root = std::move(*parsed);
    if (!root.is_object())
        return std::unexpected("theme root must be an object");
    constexpr std::array<std::string_view, 8>
        allowed{"$schema", "schema", "name", "color", "space", "radius", "type", "motion"};
    for (const auto& [name, unused] : root.items()) {
        static_cast<void>(unused);
        if (std::ranges::find(allowed, name) == allowed.end())
            return std::unexpected("theme contains unknown key: " + name);
    }
    if (!root["$schema"].is_string() && root.value("schema", 0) != 1)
        return std::unexpected("theme requires $schema (legacy schema 1 is accepted during migration)");
    if (!root["name"].is_string() || root["name"].get<std::string>().empty())
        return std::unexpected("theme name must be a non-empty string");

    Theme theme = Default();
    {
        for (const auto& [name, value] : root["color"].items()) {
            auto color = ParseColor(value.get<std::string>());
            if (!color)
                return std::unexpected("color." + name + ": " + color.error());
            theme.colors_.insert_or_assign(name, *color);
        }
        for (const auto& [name, value] : root["space"].items()) {
            theme.spaces_.insert_or_assign(name, value.get<f32>());
        }
        for (const auto& [name, value] : root["radius"].items()) {
            theme.radii_.insert_or_assign(name, value.get<f32>());
        }
        for (const auto& [name, value] : root["type"].items()) {
            theme.types_.insert_or_assign(
                name,
                TypeStyle{
                    .family = value.value("family", "sans"),
                    .size = value.value("size", 14.0f),
                    .line = value.value("line", 1.2f),
                    .weight = value.value("weight", 400),
                    .tracking = value.value("tracking", 0.0f),
                }
            );
        }
        for (const auto& [name, value] : root["motion"].items()) {
            constexpr std::array<std::string_view, 3> motion_keys{"duration", "delay", "curve"};
            for (const auto& [key, unused] : value.items()) {
                static_cast<void>(unused);
                if (std::ranges::find(motion_keys, key) == motion_keys.end())
                    return std::unexpected("motion." + name + " contains unknown key: " + key);
            }
            const auto curve = ParseCurve(value.value("curve", "out"));
            if (!curve)
                return std::unexpected("motion." + name + ".curve must be linear, in, out, or in-out");
            theme.motions_.insert_or_assign(
                name,
                Transition{
                    .duration = std::chrono::milliseconds{value.value("duration", 120)},
                    .delay = std::chrono::milliseconds{value.value("delay", 0)},
                    .curve = *curve,
                }
            );
        }
    }
    return theme;
}

Color Theme::ColorOf(std::string_view name, Color fallback) const {
    return Find(colors_, name, fallback);
}

f32 Theme::Space(std::string_view name, f32 fallback) const {
    return Find(spaces_, name, fallback);
}

f32 Theme::RadiusOf(std::string_view name, f32 fallback) const {
    return Find(radii_, name, fallback);
}

TypeStyle Theme::Type(std::string_view name, TypeStyle fallback) const {
    return Find(types_, name, std::move(fallback));
}

Transition Theme::MotionOf(std::string_view name, Transition fallback) const {
    return Find(motions_, name, fallback);
}

std::expected<bool, std::string> ThemeStore::Reload(std::string_view jsonc) {
    auto parsed = Theme::Parse(jsonc);
    if (!parsed)
        return std::unexpected(parsed.error());
    theme_ = std::move(*parsed);
    ++revision_;
    return true;
}

} // namespace woki::ui
