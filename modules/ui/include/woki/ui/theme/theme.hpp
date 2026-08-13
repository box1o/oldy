#pragma once

#include <string>
#include <expected>
#include <string_view>
#include <unordered_map>

#include "../motion/motion.hpp"

namespace woki::ui {

struct TypeStyle {
    std::string family;
    f32 size{14.0f};
    f32 line{1.2f};
    i32 weight{400};
    f32 tracking{};
};

class Theme {
public:
    [[nodiscard]] static const Theme& Default();
    [[nodiscard]] static std::expected<Theme, std::string> Parse(std::string_view jsonc);

    [[nodiscard]] Color ColorOf(std::string_view name, Color fallback = {}) const;
    [[nodiscard]] f32 Space(std::string_view name, f32 fallback = 0.0f) const;
    [[nodiscard]] f32 RadiusOf(std::string_view name, f32 fallback = 0.0f) const;
    [[nodiscard]] TypeStyle Type(std::string_view name, TypeStyle fallback = {}) const;
    [[nodiscard]] Transition MotionOf(std::string_view name, Transition fallback = {}) const;

private:
    std::unordered_map<std::string, Color> colors_;
    std::unordered_map<std::string, f32> spaces_;
    std::unordered_map<std::string, f32> radii_;
    std::unordered_map<std::string, TypeStyle> types_;
    std::unordered_map<std::string, Transition> motions_;
};

class ThemeStore {
public:
    [[nodiscard]] std::expected<bool, std::string> Reload(std::string_view jsonc);

    [[nodiscard]] const Theme& Current() const {
        return theme_;
    }

    [[nodiscard]] u64 Revision() const {
        return revision_;
    }

private:
    Theme theme_{Theme::Default()};
    u64 revision_{};
};

} // namespace woki::ui
