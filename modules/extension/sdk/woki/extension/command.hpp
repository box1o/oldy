#pragma once

#include <woki/extension/types.hpp>

namespace woki::extension {

class Command final {
public:
    constexpr Command(StringView id, Bytes payload) noexcept
        : id_(id),
          payload_(payload) {}

    [[nodiscard]] constexpr StringView Id() const noexcept {
        return id_;
    }

    [[nodiscard]] constexpr Bytes Payload() const noexcept {
        return payload_;
    }

private:
    StringView id_{};
    Bytes payload_{};
};

} // namespace woki::extension
