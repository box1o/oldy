#pragma once

#include <limits>
#include <compare>

#include <woki/core.hpp>

namespace woki {

class Entity final {
public:
    static constexpr u32 kInvalidIndex = std::numeric_limits<u32>::max();
    static constexpr u32 kInvalidGeneration = std::numeric_limits<u32>::max();

    constexpr Entity() noexcept = default;

    explicit constexpr Entity(u32 index, u32 generation = 0) noexcept
        : index_(index),
          generation_(generation) {}

    explicit constexpr Entity(u64 value) noexcept
        : index_(static_cast<u32>(value & 0xFFFFFFFFull)),
          generation_(static_cast<u32>((value >> 32u) & 0xFFFFFFFFull)) {}

    [[nodiscard]] static constexpr Entity Null() noexcept {
        return Entity();
    }

    [[nodiscard]] static constexpr Entity FromRaw(u64 value) noexcept {
        return Entity(value);
    }

    [[nodiscard]] constexpr u32 Index() const noexcept {
        return index_;
    }

    [[nodiscard]] constexpr u32 Generation() const noexcept {
        return generation_;
    }

    [[nodiscard]] constexpr u64 Value() const noexcept {
        return (static_cast<u64>(generation_) << 32u) | static_cast<u64>(index_);
    }

    [[nodiscard]] constexpr bool Valid() const noexcept {
        return index_ != kInvalidIndex && generation_ != kInvalidGeneration;
    }

    [[nodiscard]] explicit constexpr operator bool() const noexcept {
        return Valid();
    }

    [[nodiscard]] constexpr auto operator<=>(const Entity&) const noexcept = default;

private:
    u32 index_ = kInvalidIndex;
    u32 generation_ = kInvalidGeneration;
};

} // namespace woki
