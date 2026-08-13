#pragma once

// IWYU pragma: private, include "woki/asset.hpp"

#include <compare>
#include <string>
#include <string_view>

#include <woki/core.hpp>

#include "path.hpp"

namespace woki::asset {

enum class AssetScheme : u8 { Engine, Project, Plugin, Cache };

class AssetUri {
public:
    AssetUri() = default;
    [[nodiscard]] static Result<AssetUri> Parse(std::string_view uri);
    [[nodiscard]] static AssetUri Engine(const AssetPath& path);

    [[nodiscard]] AssetScheme Scheme() const noexcept {
        return scheme_;
    }

    [[nodiscard]] const std::string& Authority() const noexcept {
        return authority_;
    }

    [[nodiscard]] const AssetPath& Path() const noexcept {
        return path_;
    }

    [[nodiscard]] const std::string& String() const noexcept {
        return uri_;
    }

    [[nodiscard]] friend auto operator<=>(const AssetUri&, const AssetUri&) noexcept = default;

private:
    AssetUri(AssetScheme scheme, std::string authority, AssetPath path, std::string uri)
        : scheme_(scheme),
          authority_(std::move(authority)),
          path_(std::move(path)),
          uri_(std::move(uri)) {}

    AssetScheme scheme_{};
    std::string authority_;
    AssetPath path_;
    std::string uri_;
};

} // namespace woki::asset

template <>
struct std::hash<woki::asset::AssetUri> {
    [[nodiscard]] std::size_t operator()(const woki::asset::AssetUri& value) const noexcept {
        return std::hash<std::string>{}(value.String());
    }
};
