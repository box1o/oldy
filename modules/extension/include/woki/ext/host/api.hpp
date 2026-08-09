#pragma once

// IWYU pragma: private, include "woki/ext/ext.hpp"

#include <span>
#include <vector>
#include <filesystem>
#include <string_view>

#include "../registry.hpp"

namespace woki::ext::host {

enum class LogLevel : u8 {
    Debug,
    Info,
    Warn,
    Error,
};

class HostApi final {
public:
    explicit HostApi(Record& record) noexcept;

    void Log(LogLevel level, std::string_view message) const;

    [[nodiscard]] Result<std::filesystem::path> DataPath() const;
    [[nodiscard]] Result<std::filesystem::path> CachePath() const;

    [[nodiscard]] Result<std::vector<u8>> ReadFile(const std::filesystem::path& relative_path) const;
    [[nodiscard]] Result<void> WriteFile(const std::filesystem::path& relative_path, std::span<const u8> data) const;
    [[nodiscard]] Result<void> AppendFile(const std::filesystem::path& relative_path, std::span<const u8> data) const;
    [[nodiscard]] Result<std::string> ReadConfig(std::string_view key) const;
    [[nodiscard]] Result<void> WriteConfig(std::string_view key, std::string_view value) const;

private:
    [[nodiscard]] Result<void> Require(Permission permission) const;
    [[nodiscard]] Result<std::filesystem::path> ResolveDataFile(const std::filesystem::path& relative_path) const;
    [[nodiscard]] Result<std::filesystem::path> ResolveConfigFile(std::string_view key) const;

    Record* record_;
};

} // namespace woki::ext::host
