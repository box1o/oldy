#pragma once

// IWYU pragma: private, include "woki/core.hpp"

#include <cstdlib>

#include "../logger/logger.hpp"

namespace woki::detail {

[[noreturn]] inline void AssertFailed(const char* expression, const char* file, int line, const char* message = nullptr) {
    if (message != nullptr) {
        slog::Critical("Assertion failed: '{}' at {}:{} ({})", expression, file, line, message);
    } else {
        slog::Critical("Assertion failed: '{}' at {}:{}", expression, file, line);
    }
    std::abort();
}

[[noreturn]] inline void Panic(const char* file, int line, const char* message) {
    slog::Critical("Panic at {}:{} ({})", file, line, message != nullptr ? message : "no message");
    std::abort();
}

} // namespace woki::detail

#ifndef WOKI_ASSERT
#if defined(NDEBUG)
#define WOKI_ASSERT(expr) ((void)0)
#define WOKI_ASSERT_MSG(expr, msg) ((void)0)
#else
#define WOKI_ASSERT(expr)                                                                                                                                                                                                  \
    do {                                                                                                                                                                                                                   \
        if (!(expr)) {                                                                                                                                                                                                     \
            ::woki::detail::AssertFailed(#expr, __FILE__, __LINE__);                                                                                                                                                       \
        }                                                                                                                                                                                                                  \
    } while (0)
#define WOKI_ASSERT_MSG(expr, msg)                                                                                                                                                                                         \
    do {                                                                                                                                                                                                                   \
        if (!(expr)) {                                                                                                                                                                                                     \
            ::woki::detail::AssertFailed(#expr, __FILE__, __LINE__, (msg));                                                                                                                                                \
        }                                                                                                                                                                                                                  \
    } while (0)
#endif
#endif

#define WOKI_PANIC(msg) ::woki::detail::Panic(__FILE__, __LINE__, (msg))
