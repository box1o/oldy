#pragma once

#if __has_include(<type_traits>)
#include <type_traits>
#endif

namespace woki::ext::detail {

template <typename T>
T&& Declval() noexcept;

#if __has_include(<type_traits>)
template <typename T, typename U>
inline constexpr bool IsSame = std::is_same_v<T, U>;
template <typename T>
inline constexpr bool IsTriviallyDefaultConstructible = std::is_trivially_default_constructible_v<T>;
template <typename T>
inline constexpr bool IsTriviallyDestructible = std::is_trivially_destructible_v<T>;
#else
// Bare wasm targets may not ship C++ library headers even though this trait is
// entirely compile-time. Keep the equivalent portable specialization local.
template <typename T, typename U>
inline constexpr bool IsSame = false;

template <typename T>
inline constexpr bool IsSame<T, T> = true;

template <typename T>
inline constexpr bool IsTriviallyDefaultConstructible = __is_trivially_constructible(T);
template <typename T>
inline constexpr bool IsTriviallyDestructible = __is_trivially_destructible(T);
#endif

} // namespace woki::ext::detail
