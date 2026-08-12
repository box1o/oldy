#pragma once

// IWYU pragma: private, include "woki/core.hpp"

#include <compare>
#include <cstddef>
#include <functional>
#include <limits>

#include "types.hpp"

namespace woki {

template <typename Tag>
class Handle {
public:
    constexpr Handle() noexcept = default;

    [[nodiscard]] static constexpr Handle Create(const u32 index, const u32 generation) noexcept {
        return Handle(index, generation);
    }

    [[nodiscard]] constexpr bool IsValid() const noexcept {
        return index_ != kInvalid;
    }

    [[nodiscard]] constexpr u32 Index() const noexcept {
        return index_;
    }

    [[nodiscard]] constexpr u32 Generation() const noexcept {
        return generation_;
    }

    [[nodiscard]] friend constexpr auto operator<=>(const Handle&, const Handle&) noexcept = default;

private:
    static constexpr u32 kInvalid = std::numeric_limits<u32>::max();

    constexpr Handle(const u32 index, const u32 generation) noexcept
        : index_(index),
          generation_(generation) {}

    u32 index_{kInvalid};
    u32 generation_{0};
};

} // namespace woki

namespace std {

template <typename Tag>
struct hash<woki::Handle<Tag>> {
    [[nodiscard]] std::size_t operator()(const woki::Handle<Tag> handle) const noexcept {
        const auto value = (static_cast<woki::u64>(handle.Generation()) << 32U) | handle.Index();
        return std::hash<woki::u64>{}(value);
    }
};

} // namespace std
