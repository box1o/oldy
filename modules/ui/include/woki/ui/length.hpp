#pragma once

#include <variant>
#include <algorithm>

#include <woki/types/types.hpp>

namespace woki::ui {

struct Auto {
    [[nodiscard]] bool operator==(const Auto&) const = default;
};

struct Grow {
    f32 factor{1.0f};
    [[nodiscard]] bool operator==(const Grow&) const = default;
};

struct Px {
    f32 value{};
    [[nodiscard]] bool operator==(const Px&) const = default;
};

struct Percent {
    f32 value{};
    [[nodiscard]] bool operator==(const Percent&) const = default;
};

class Length {
public:
    Length() = default;

    Length(Auto value)
        : value_(value) {}

    Length(Grow value)
        : value_(value) {}

    Length(Px value)
        : value_(value) {}

    Length(Percent value)
        : value_(value) {}

    [[nodiscard]] bool IsAuto() const {
        return std::holds_alternative<Auto>(value_);
    }

    [[nodiscard]] bool IsGrow() const {
        return std::holds_alternative<Grow>(value_);
    }

    [[nodiscard]] f32 Factor() const;
    [[nodiscard]] f32 Resolve(f32 available, f32 intrinsic) const;
    [[nodiscard]] bool operator==(const Length&) const = default;

private:
    std::variant<Auto, Grow, Px, Percent> value_{Auto{}};
};

} // namespace woki::ui
