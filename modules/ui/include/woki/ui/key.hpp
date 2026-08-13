#pragma once

#include <functional>
#include <string_view>

#include <woki/types/types.hpp>

namespace woki::ui {

class Key {
public:
    constexpr Key() = default;

    constexpr explicit Key(u64 value)
        : value_(value) {}

    [[nodiscard]] static Key From(std::string_view value) {
        return Key{static_cast<u64>(std::hash<std::string_view>{}(value))};
    }

    [[nodiscard]] constexpr u64 Value() const {
        return value_;
    }

    [[nodiscard]] constexpr explicit operator bool() const {
        return value_ != 0;
    }

    [[nodiscard]] constexpr bool operator==(const Key&) const = default;

private:
    u64 value_{};
};

} // namespace woki::ui
