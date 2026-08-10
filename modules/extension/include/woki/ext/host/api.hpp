#pragma once

// Host implementation detail. This header is not installed.

#include <span>
#include <vector>
#include <filesystem>
#include <string_view>

#include "../perm.hpp"
#include "../package.hpp"
#include "../internal/event_service.hpp"

namespace woki::ext::host {

class EventSession;
class EventService;

enum class LogLevel : u8 {
    Debug,
    Info,
    Warn,
    Error,
};

struct Context {
    std::string extension_id;
    std::vector<Permission> granted_permissions;
    std::filesystem::path data_root;
    std::filesystem::path config_root;
    std::filesystem::path cache_root;
    std::shared_ptr<EventSession> event_session;
    std::shared_ptr<EventService> event_service;
};

class HostApi final {
public:
    explicit HostApi(Context context) noexcept;

    [[nodiscard]] bool Allows(Permission permission) const noexcept;
    void Log(LogLevel level, std::string_view message) const;

    [[nodiscard]] Result<std::filesystem::path> DataPath() const;
    [[nodiscard]] Result<std::filesystem::path> CachePath() const;

    [[nodiscard]] Result<std::vector<u8>> ReadFile(const std::filesystem::path& relative_path) const;
    [[nodiscard]] Result<void> WriteFile(const std::filesystem::path& relative_path, std::span<const u8> data) const;
    [[nodiscard]] Result<void> AppendFile(const std::filesystem::path& relative_path, std::span<const u8> data) const;
    [[nodiscard]] Result<std::string> ReadConfig(std::string_view key) const;
    [[nodiscard]] Result<void> WriteConfig(std::string_view key, std::string_view value) const;
    [[nodiscard]] Result<void> SubscribeEvent(u32 event_type) const;
    [[nodiscard]] Result<void> EmitEvent(u32 event_type, std::span<const u8> payload) const;
    [[nodiscard]] Result<void> SubscribeNamedEvent(std::string_view topic) const;
    [[nodiscard]] Result<void> EmitNamedEvent(std::string_view topic, std::span<const u8> payload) const;

private:
    [[nodiscard]] Result<void> Require(Permission permission) const;
    [[nodiscard]] Result<std::filesystem::path> ResolveDataFile(const std::filesystem::path& relative_path) const;
    [[nodiscard]] Result<std::filesystem::path> ResolveConfigFile(std::string_view key) const;

    Context context_;
};

} // namespace woki::ext::host
