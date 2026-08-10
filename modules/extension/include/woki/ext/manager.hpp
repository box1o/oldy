#pragma once

#include <span>
#include <filesystem>
#include <string_view>

#include <woki/core.hpp>

#include "policy.hpp"
#include "status.hpp"
#include "command.hpp"
#include "package.hpp"
#include "host/event_bus.hpp"

namespace woki::ext {

class RuntimeEngine;
class ExtensionManager;

struct HostOptions {
    host::EventBus* event_bus{nullptr};
    /// Allows synchronous web execution only for wasm the host has independently trusted.
    bool allow_trusted_synchronous_web{false};
};

namespace internal {
struct ExtensionManagerAccess;
}

[[nodiscard]] scope<ExtensionManager> CreateExtensionManager(HostOptions options = {});

/// Discovers, loads, and coordinates extensions for one host application.
/// All methods must be called from the thread that owns the manager.
class ExtensionManager final {
public:
    ~ExtensionManager();

    ExtensionManager(const ExtensionManager&) = delete;
    ExtensionManager& operator=(const ExtensionManager&) = delete;

    /// Binds a non-owning event sink. The sink must outlive this manager or be unbound.
    void SetEventBus(host::EventBus* bus) noexcept;
    void SetCapabilityPolicy(scope<CapabilityPolicy> policy);
    void SetRoots(Roots roots);

    [[nodiscard]] Result<PackageLayout> Install(const std::filesystem::path& package_path);
    [[nodiscard]] Result<PackageLayout> InstallUnpacked(const std::filesystem::path& source_root);
    [[nodiscard]] Result<void> Scan();
    [[nodiscard]] Result<void> ScanSource(const std::filesystem::path& source_root);
    [[nodiscard]] Result<void> Load(std::string_view id);
    [[nodiscard]] Result<void> LoadAll();
    [[nodiscard]] Result<void> ActivateStartup();

    void Tick(f64 delta_ms);
    void DispatchEvent(u32 event_type, std::span<const u8> payload);
    void DispatchNamedEvent(std::string_view topic, std::span<const u8> payload);
    [[nodiscard]] Result<void> ExecuteCommand(std::string_view command_id, std::span<const u8> payload = {});
    void Unload(std::string_view id);
    void UnloadAll();

    [[nodiscard]] std::span<const ExtensionPackage> Packages() const noexcept;
    [[nodiscard]] std::span<const DiscoveryFailure> Failures() const noexcept;
    [[nodiscard]] std::span<const ExtensionStatus> Statuses() const noexcept;
    [[nodiscard]] std::span<const CommandRecord> Commands() const noexcept;
    [[nodiscard]] const ExtensionPackage* Find(std::string_view id) const noexcept;

private:
    explicit ExtensionManager(scope<RuntimeEngine> engine) noexcept;

    struct Impl;
    scope<Impl> impl_;

    friend scope<ExtensionManager> CreateExtensionManager(HostOptions options);
    friend struct internal::ExtensionManagerAccess;
};

} // namespace woki::ext
