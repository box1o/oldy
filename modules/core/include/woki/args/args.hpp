#pragma once

// IWYU pragma: private, include "woki/core.hpp"

#include <string>
#include <vector>
#include <sstream>
#include <utility>
#include <optional>
#include <exception>
#include <cxxopts.hpp>
#include <string_view>
#include <type_traits>

#include "../error/result.hpp"

namespace woki {

namespace detail {

template <typename T>
[[nodiscard]] inline std::string OptionDefaultValue(const T& value) {
    if constexpr (std::same_as<std::decay_t<T>, std::string>) {
        return value;
    } else if constexpr (std::same_as<std::decay_t<T>, std::string_view>) {
        return std::string(value);
    } else if constexpr (std::same_as<std::decay_t<T>, const char*>) {
        return value != nullptr ? std::string(value) : std::string();
    } else if constexpr (std::same_as<std::decay_t<T>, bool>) {
        return value ? "true" : "false";
    } else {
        std::ostringstream stream;
        stream << value;
        return stream.str();
    }
}

} // namespace detail

class ArgumentParser {
public:
    explicit ArgumentParser(std::string program = "woki", std::string description = {});

    void AddFlag(std::string option, std::string description);

    template <typename T>
    void AddOption(std::string option, std::string description) {
        options_.add_options()(std::move(option), std::move(description), cxxopts::value<T>());
    }

    template <typename T>
    void AddOption(std::string option, std::string description, const T& default_value) {
        options_.add_options()(std::move(option), std::move(description), cxxopts::value<T>()->default_value(detail::OptionDefaultValue(default_value)));
    }

    void AddPositional(std::string option);
    void SetPositionalHelp(std::string help);

    [[nodiscard]] Result<void> Parse(int argc, char** argv);

    [[nodiscard]] bool Parsed() const noexcept;
    [[nodiscard]] bool Has(std::string_view key) const;

    template <typename T>
    [[nodiscard]] auto Get(std::string_view key) const -> Result<T> {
        if (!parse_result_.has_value()) {
            return Err(ErrorCode::InvalidState, "ArgumentParser used before Parse()");
        }

        try {
            return Ok(parse_result_->operator[](std::string(key)).as<T>());
        } catch (const std::exception& exception) {
            return Err(ErrorCode::OutOfRange, exception.what());
        }
    }

    [[nodiscard]] std::string Help() const;

private:
    cxxopts::Options options_;
    std::optional<cxxopts::ParseResult> parse_result_;
    std::vector<std::string> positional_;
    std::string positional_help_;
};

} // namespace woki
