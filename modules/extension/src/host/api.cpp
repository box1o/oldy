#include <string>
#include <fstream>
#include <iterator>
#include <algorithm>
#include <filesystem>
#include <system_error>

#include "woki/ext/perm.hpp"
#include "woki/ext/limits.hpp"
#include "woki/ext/host/api.hpp"
#include <woki/logger/logger.hpp>
#include "woki/ext/path_safety.hpp"

namespace woki::ext::host {

namespace {

namespace fs = std::filesystem;

[[nodiscard]] bool IsSafeConfigKey(std::string_view key) {
    if (key.empty() || key.size() > limits::kMaxConfigKeyBytes) {
        return false;
    }
    return std::ranges::all_of(key, [](char ch) { return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') || ch == '_' || ch == '-' || ch == '.'; });
}

[[nodiscard]] Result<void> EnsureDirectory(const fs::path& path) {
    std::error_code error;
    fs::create_directories(path, error);
    if (error) {
        return Err(ErrorCode::FileWriteError, error.message());
    }
    return Ok();
}

[[nodiscard]] Result<void> EnsureParentDirectory(const fs::path& path) {
    const fs::path parent = path.parent_path();
    if (parent.empty()) {
        return Ok();
    }
    return EnsureDirectory(parent);
}

} // namespace

HostApi::HostApi(Record& record) noexcept
    : record_(&record) {}

void HostApi::Log(LogLevel level, std::string_view message) const {
    if (!Require(Permission::Log)) {
        return;
    }

    if (message.size() > limits::kMaxLogBytes) {
        message = message.substr(0, limits::kMaxLogBytes);
    }

    switch (level) {
        case LogLevel::Debug:
            slog::Debug("[{}] {}", record_->id, message);
            break;
        case LogLevel::Info:
            slog::Info("[{}] {}", record_->id, message);
            break;
        case LogLevel::Warn:
            slog::Warn("[{}] {}", record_->id, message);
            break;
        case LogLevel::Error:
            slog::Error("[{}] {}", record_->id, message);
            break;
    }
}

Result<fs::path> HostApi::DataPath() const {
    auto allowed = Require(Permission::Paths);
    if (!allowed) {
        return Err(allowed.error());
    }
    return Ok(record_->package.data_root);
}

Result<fs::path> HostApi::CachePath() const {
    auto allowed = Require(Permission::Paths);
    if (!allowed) {
        return Err(allowed.error());
    }
    return Ok(record_->package.cache_root);
}

Result<std::vector<u8>> HostApi::ReadFile(const fs::path& relative_path) const {
    auto file = ResolveDataFile(relative_path);
    if (!file) {
        return Err(file.error());
    }

    std::error_code error;
    const auto size = fs::file_size(*file, error);
    if (error) {
        return Err(ErrorCode::FileNotFound, error.message());
    }
    if (size > limits::kMaxFileBytes) {
        return Err(ErrorCode::ValidationOutOfRange, "Extension file read exceeds 16 MiB limit.");
    }

    std::ifstream input(*file, std::ios::binary);
    if (!input.good()) {
        return Err(ErrorCode::FileReadError, "Failed to open extension data file for reading.");
    }

    std::vector<u8> data;
    data.reserve(static_cast<std::size_t>(size));
    std::ranges::copy(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>(), std::back_inserter(data));
    return Ok(std::move(data));
}

Result<void> HostApi::WriteFile(const fs::path& relative_path, std::span<const u8> data) const {
    if (data.size() > limits::kMaxFileBytes) {
        return Err(ErrorCode::ValidationOutOfRange, "Extension file write exceeds 16 MiB limit.");
    }

    auto file = ResolveDataFile(relative_path);
    if (!file) {
        return Err(file.error());
    }

    auto parent = EnsureParentDirectory(*file);
    if (!parent) {
        return Err(parent.error());
    }

    std::ofstream output(*file, std::ios::binary | std::ios::trunc);
    if (!output.good()) {
        return Err(ErrorCode::FileWriteError, "Failed to open extension data file for writing.");
    }

    output.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
    if (!output.good()) {
        return Err(ErrorCode::FileWriteError, "Failed to write extension data file.");
    }

    return Ok();
}

Result<void> HostApi::AppendFile(const fs::path& relative_path, std::span<const u8> data) const {
    if (data.size() > limits::kMaxFileBytes) {
        return Err(ErrorCode::ValidationOutOfRange, "Extension file append exceeds 16 MiB limit.");
    }

    auto file = ResolveDataFile(relative_path);
    if (!file) {
        return Err(file.error());
    }

    auto parent = EnsureParentDirectory(*file);
    if (!parent) {
        return Err(parent.error());
    }

    std::ofstream output(*file, std::ios::binary | std::ios::app);
    if (!output.good()) {
        return Err(ErrorCode::FileWriteError, "Failed to open extension data file for appending.");
    }

    output.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
    if (!output.good()) {
        return Err(ErrorCode::FileWriteError, "Failed to append extension data file.");
    }

    return Ok();
}

Result<std::string> HostApi::ReadConfig(std::string_view key) const {
    auto file = ResolveConfigFile(key);
    if (!file) {
        return Err(file.error());
    }

    std::error_code error;
    const auto size = fs::file_size(*file, error);
    if (error) {
        return Err(ErrorCode::FileNotFound, error.message());
    }
    if (size > limits::kMaxConfigValueBytes) {
        return Err(ErrorCode::ValidationOutOfRange, "Extension config value exceeds 64 KiB limit.");
    }

    std::ifstream input(*file, std::ios::binary);
    if (!input.good()) {
        return Err(ErrorCode::FileReadError, "Failed to open extension config file for reading.");
    }
    return Ok(std::string{std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()});
}

Result<void> HostApi::WriteConfig(std::string_view key, std::string_view value) const {
    if (value.size() > limits::kMaxConfigValueBytes) {
        return Err(ErrorCode::ValidationOutOfRange, "Extension config value exceeds 64 KiB limit.");
    }

    auto file = ResolveConfigFile(key);
    if (!file) {
        return Err(file.error());
    }

    auto parent = EnsureParentDirectory(*file);
    if (!parent) {
        return Err(parent.error());
    }

    std::ofstream output(*file, std::ios::binary | std::ios::trunc);
    if (!output.good()) {
        return Err(ErrorCode::FileWriteError, "Failed to open extension config file for writing.");
    }

    output.write(value.data(), static_cast<std::streamsize>(value.size()));
    if (!output.good()) {
        return Err(ErrorCode::FileWriteError, "Failed to write extension config file.");
    }

    return Ok();
}

Result<void> HostApi::Require(Permission permission) const {
    if (record_ == nullptr) {
        return Err(ErrorCode::InvalidState, "HostApi has no active extension record.");
    }
    if (!HasPermission(record_->manifest, permission)) {
        return Err(ErrorCode::ValidationInvalidState, "Extension '" + record_->id + "' does not declare permission '" + std::string(ToString(permission)) + "'. Add it to manifest.yaml permissions.");
    }
    return Ok();
}

Result<fs::path> HostApi::ResolveDataFile(const fs::path& relative_path) const {
    auto allowed = Require(Permission::Storage);
    if (!allowed) {
        return Err(allowed.error());
    }
    if (!IsSafeRelativePath(relative_path)) {
        return Err(ErrorCode::ValidationInvalidState, "Extension storage path must be relative and must not contain '..'.");
    }

    return Ok((record_->package.data_root / relative_path).lexically_normal());
}

Result<fs::path> HostApi::ResolveConfigFile(std::string_view key) const {
    auto allowed = Require(Permission::Config);
    if (!allowed) {
        return Err(allowed.error());
    }
    if (!IsSafeConfigKey(key)) {
        return Err(ErrorCode::ValidationInvalidState, "Extension config key must use only letters, digits, '.', '_' or '-'.");
    }

    return Ok((record_->package.data_root / "config" / std::string(key)).lexically_normal());
}

} // namespace woki::ext::host
