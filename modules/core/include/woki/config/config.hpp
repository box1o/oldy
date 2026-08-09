#pragma once

// IWYU pragma: private, include "woki/core.hpp"

#include <string>
#include <cstdlib>
#include <charconv>
#include <filesystem>
#include <string_view>
#include <type_traits>
#include <unordered_map>

#include "../error/result.hpp"

namespace woki {

enum class BuildMode : u8 {
    Debug = 0,
    Release,
};

[[nodiscard]] constexpr const char* ToString(BuildMode mode) noexcept {
    switch (mode) {
        case BuildMode::Debug:
            return "Debug";
        case BuildMode::Release:
            return "Release";
    }

    return "Unknown";
}

class BuildConfig {
public:
    [[nodiscard]] static constexpr BuildMode Mode() noexcept {
#ifdef NDEBUG
        return BuildMode::Release;
#else
        return BuildMode::Debug;
#endif
    }

    [[nodiscard]] static constexpr bool IsDebug() noexcept {
        return Mode() == BuildMode::Debug;
    }

    [[nodiscard]] static constexpr bool IsRelease() noexcept {
        return Mode() == BuildMode::Release;
    }
};

namespace detail {

template <typename T>
inline constexpr bool kAlwaysFalse = false;

template <typename T>
[[nodiscard]] inline auto ParseConfigValue(std::string_view value) -> Result<T> {
    using Decayed = std::decay_t<T>;

    if constexpr (std::same_as<Decayed, std::string>) {
        return Ok(std::string(value));
    } else if constexpr (std::same_as<Decayed, std::filesystem::path>) {
        return Ok(std::filesystem::path(value));
    } else if constexpr (std::same_as<Decayed, bool>) {
        if (value == "1" || value == "true" || value == "TRUE" || value == "yes" || value == "on") {
            return Ok(true);
        }
        if (value == "0" || value == "false" || value == "FALSE" || value == "no" || value == "off") {
            return Ok(false);
        }
        return Err(ErrorCode::ParseInvalidFormat, "Failed to parse bool config value");
    } else if constexpr (std::integral<Decayed>) {
        Decayed parsed{};
        const auto* begin = value.data();
        const auto* end = value.data() + value.size();
        const auto result = std::from_chars(begin, end, parsed);
        if (result.ec != std::errc{} || result.ptr != end) {
            return Err(ErrorCode::ParseInvalidFormat, "Failed to parse integer config value");
        }
        return Ok(parsed);
    } else if constexpr (std::floating_point<Decayed>) {
        std::string owned(value);
        char* parse_end = nullptr;

        if constexpr (std::same_as<Decayed, float>) {
            const float parsed = std::strtof(owned.c_str(), &parse_end);
            if (parse_end == nullptr || *parse_end != '\0') {
                return Err(ErrorCode::ParseInvalidFormat, "Failed to parse float config value");
            }
            return Ok(parsed);
        } else if constexpr (std::same_as<Decayed, double>) {
            const double parsed = std::strtod(owned.c_str(), &parse_end);
            if (parse_end == nullptr || *parse_end != '\0') {
                return Err(ErrorCode::ParseInvalidFormat, "Failed to parse double config value");
            }
            return Ok(parsed);
        } else {
            const long double parsed = std::strtold(owned.c_str(), &parse_end);
            if (parse_end == nullptr || *parse_end != '\0') {
                return Err(ErrorCode::ParseInvalidFormat, "Failed to parse long double config value");
            }
            return Ok(static_cast<Decayed>(parsed));
        }
    } else {
        static_assert(kAlwaysFalse<T>, "Unsupported config value type");
    }
}

} // namespace detail

class Config {
public:
    [[nodiscard]] static Result<Config> LoadFromYamlFile(const std::filesystem::path& path);

    void Set(std::string_view key, std::string_view value);

    [[nodiscard]] bool Has(std::string_view key) const;
    void Remove(std::string_view key);
    void Clear();

    [[nodiscard]] std::size_t Size() const noexcept;
    [[nodiscard]] bool Empty() const noexcept;

    [[nodiscard]] Result<std::string_view> GetStringView(std::string_view key) const;
    [[nodiscard]] Result<std::string> GetString(std::string_view key) const;

    template <typename T>
    [[nodiscard]] auto Get(std::string_view key) const -> Result<T> {
        auto raw_value = GetStringView(key);
        if (!raw_value) {
            return Err(raw_value.error());
        }

        return detail::ParseConfigValue<T>(*raw_value);
    }

    template <typename T>
    [[nodiscard]] T GetOr(std::string_view key, T default_value) const {
        auto value = Get<T>(key);
        if (!value) {
            return default_value;
        }

        return *value;
    }

    void Merge(const Config& other);

    [[nodiscard]] Result<void> LoadYaml(const std::filesystem::path& path);

private:
    std::unordered_map<std::string, std::string> values_;
};

} // namespace woki
