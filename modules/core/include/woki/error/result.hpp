#pragma once

// IWYU pragma: private, include "woki/core.hpp"

#include <cstdio>
#include <cstdlib>
#include <utility>
#include <concepts>
#include <expected>
#include <type_traits>
#include <source_location>

#include "errors.hpp"

namespace woki {

template <typename T>
using Result = std::expected<T, Error>;

template <typename T>
requires(!std::same_as<std::decay_t<T>, std::unexpected<Error>>)
[[nodiscard]] constexpr auto Ok(T&& value) -> Result<std::decay_t<T>> {
    return Result<std::decay_t<T>>(std::forward<T>(value));
}

[[nodiscard]] constexpr auto Ok() noexcept -> Result<void> {
    return Result<void>();
}

[[nodiscard]] inline auto MakeError(ErrorCode code, std::string_view message = {}, std::source_location location = std::source_location::current()) -> Error {
    return Error(code, message, location);
}

[[nodiscard]] inline auto Err(ErrorCode code, std::string_view message, std::source_location location = std::source_location::current()) -> std::unexpected<Error> {
    return std::unexpected<Error>(MakeError(code, message, location));
}

[[nodiscard]] inline auto Err(ErrorCode code, std::source_location location = std::source_location::current()) -> std::unexpected<Error> {
    return std::unexpected<Error>(MakeError(code, {}, location));
}

[[nodiscard]] inline auto Err(Error error) -> std::unexpected<Error> {
    return std::unexpected<Error>(std::move(error));
}

namespace detail {

template <typename T>
[[nodiscard]] auto Unwrap(Result<T>&& result) noexcept(std::is_nothrow_move_constructible_v<T>) -> T {
#ifndef NDEBUG
    if (!result) {
        std::abort();
    }
#endif

    return std::move(*result);
}

inline void Unwrap(Result<void>&& result) noexcept {
#ifndef NDEBUG
    if (!result) {
        std::abort();
    }
#else
    (void)result;
#endif
}

[[noreturn]] inline void AbortTryFailure(const Error& error, const char* expression, const char* file, int line) noexcept {
    const std::string_view message = error.Message();

    std::fprintf(stderr,
        "WOKI_TRY failed\n"
        "  expression: %s\n"
        "  location:   %s:%d\n"
        "  error:      %.*s\n",
        expression, file, line, static_cast<int>(message.size()), message.data());

    std::fflush(stderr);
    std::abort();
}

} // namespace detail

} // namespace woki

#define TRY_ASSIGN(lhs, expr)                                                                                                                                                                                              \
    do {                                                                                                                                                                                                                   \
        auto _woki_try_result = (expr);                                                                                                                                                                                    \
        if (!_woki_try_result) {                                                                                                                                                                                           \
            return ::woki::Err(std::move(_woki_try_result).error());                                                                                                                                                       \
        }                                                                                                                                                                                                                  \
        (lhs) = ::woki::detail::Unwrap(std::move(_woki_try_result));                                                                                                                                                       \
    } while (false)

#define TRY_VOID(expr)                                                                                                                                                                                                     \
    do {                                                                                                                                                                                                                   \
        auto _woki_try_result = (expr);                                                                                                                                                                                    \
        if (!_woki_try_result) {                                                                                                                                                                                           \
            return ::woki::Err(std::move(_woki_try_result).error());                                                                                                                                                       \
        }                                                                                                                                                                                                                  \
    } while (false)

#define TRY(expr)                                                                                                                                                                                                          \
    ([&]() {                                                                                                                                                                                                               \
        auto _woki_try_result = (expr);                                                                                                                                                                                    \
        if (!_woki_try_result) {                                                                                                                                                                                           \
            ::woki::detail::AbortTryFailure(_woki_try_result.error(), #expr, __FILE__, __LINE__);                                                                                                                          \
        }                                                                                                                                                                                                                  \
        return ::woki::detail::Unwrap(std::move(_woki_try_result));                                                                                                                                                        \
    }())
