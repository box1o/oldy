#pragma once

// IWYU pragma: private, include "woki/asset.hpp"

#include <array>
#include <compare>
#include <cstddef>
#include <functional>
#include <span>
#include <string>
#include <string_view>

#include <woki/core.hpp>

namespace woki::asset {

class AssetId {
public:
    static constexpr std::size_t kSize = 16;

    constexpr AssetId() noexcept = default;

    explicit constexpr AssetId(std::array<u8, kSize> bytes) noexcept
        : bytes_(bytes) {}

    [[nodiscard]] static Result<AssetId> Parse(std::string_view uuid);
    [[nodiscard]] static Result<AssetId> New();
    [[nodiscard]] static AssetId FromName(std::string_view name) noexcept;
    [[nodiscard]] static AssetId FromBytes(std::span<const std::byte> bytes) noexcept;

    [[nodiscard]] constexpr const std::array<u8, kSize>& Bytes() const noexcept {
        return bytes_;
    }

    [[nodiscard]] std::string String() const;
    [[nodiscard]] bool IsValid() const noexcept;

    [[nodiscard]] explicit operator bool() const noexcept {
        return IsValid();
    }

    [[nodiscard]] friend constexpr auto operator<=>(const AssetId&, const AssetId&) noexcept = default;

private:
    std::array<u8, kSize> bytes_{};
};

struct SubresourceId {
    u32 value{};
    [[nodiscard]] friend constexpr auto operator<=>(const SubresourceId&, const SubresourceId&) noexcept = default;
};

struct AssetKey {
    AssetId asset;
    SubresourceId subresource;
    [[nodiscard]] friend constexpr auto operator<=>(const AssetKey&, const AssetKey&) noexcept = default;
};

} // namespace woki::asset

template <>
struct std::hash<woki::asset::AssetId> {
    [[nodiscard]] std::size_t operator()(const woki::asset::AssetId& value) const noexcept;
};

template <>
struct std::hash<woki::asset::AssetKey> {
    [[nodiscard]] std::size_t operator()(const woki::asset::AssetKey& value) const noexcept;
};
