#pragma once

// IWYU pragma: private, include "woki/asset.hpp"

#include <string>
#include <compare>
#include <string_view>

#include <woki/core.hpp>

namespace woki::asset {

class AssetPath {
public:
    AssetPath() = default;
    [[nodiscard]] static Result<AssetPath> Parse(std::string_view path);

    [[nodiscard]] const std::string& String() const noexcept {
        return path_;
    }

    [[nodiscard]] friend auto operator<=>(const AssetPath&, const AssetPath&) noexcept = default;

private:
    explicit AssetPath(std::string path)
        : path_(std::move(path)) {}

    std::string path_;
};

} // namespace woki::asset
