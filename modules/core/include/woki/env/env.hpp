#pragma once

// IWYU pragma: private, include "woki/core.hpp"

#include <string>
#include <cstdlib>
#include <optional>
#include <string_view>

#include "../error/result.hpp"

namespace woki::env {

namespace detail {

[[nodiscard]] inline std::optional<std::string> Lookup(std::string_view name) {
#if defined(_MSC_VER)
    const std::string key(name);
    char* buffer = nullptr;
    std::size_t length = 0;
    if (_dupenv_s(&buffer, &length, key.c_str()) != 0 || buffer == nullptr || *buffer == '\0') {
        if (buffer != nullptr) {
            std::free(buffer);
        }
        return std::nullopt;
    }

    const std::string value(buffer);
    std::free(buffer);
    return value;
#else
    const std::string key(name);
    const char* value = std::getenv(key.c_str());
    if (value == nullptr || *value == '\0') {
        return std::nullopt;
    }
    return std::string(value);
#endif
}

} // namespace detail

[[nodiscard]] inline bool Has(std::string_view name) {
    return detail::Lookup(name).has_value();
}

[[nodiscard]] inline std::optional<std::string> TryGet(std::string_view name) {
    return detail::Lookup(name);
}

[[nodiscard]] inline Result<std::string> Get(std::string_view name) {
    auto value = TryGet(name);
    if (!value.has_value()) {
        return Err(ErrorCode::FileNotFound, "Environment variable not found");
    }
    return Ok(std::move(*value));
}

[[nodiscard]] inline std::string GetOr(std::string_view name, std::string default_value) {
    auto value = TryGet(name);
    if (!value.has_value()) {
        return default_value;
    }
    return *value;
}

} // namespace woki::env
