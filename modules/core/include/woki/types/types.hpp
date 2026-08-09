#pragma once

// IWYU pragma: private, include "woki/core.hpp"

#include <memory>
#include <cstdint>
#include <utility>

namespace woki {
using u8 = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;

using i8 = std::int8_t;
using i16 = std::int16_t;
using i32 = std::int32_t;
using i64 = std::int64_t;

using f32 = float;
using f64 = double;

template <typename T>
using ref = std::shared_ptr<T>;
template <typename T>
using scope = std::unique_ptr<T>;
template <typename T>
using weak = std::weak_ptr<T>;
template <typename T>
using ref_from_this = std::enable_shared_from_this<T>;

template <typename T, typename... Args>
constexpr ref<T> createRef(Args&&... args) {
    return std::make_shared<T>(std::forward<Args>(args)...);
}

template <typename T, typename... Args>
constexpr scope<T> createScope(Args&&... args) {
    return std::make_unique<T>(std::forward<Args>(args)...);
}
} // namespace woki
