#pragma once

// Host implementation detail. This header is not installed.

#include <span>
#include <memory>
#include <string>
#include <vector>
#include <string_view>

#include "status.hpp"
#include "host/api.hpp"
#include "registry.hpp"

namespace woki::ext {

class RuntimeInstance {
public:
    virtual ~RuntimeInstance() = default;

    [[nodiscard]] virtual Result<void> Initialize() = 0;
    [[nodiscard]] virtual Result<void> Tick(f64 delta_ms) = 0;
    [[nodiscard]] virtual Result<void> DispatchEvent(u32 event_type, std::span<const u8> payload) = 0;

    [[nodiscard]] virtual Result<void> DispatchNamedEvent(std::string_view, std::span<const u8>) {
        return Ok();
    }

    [[nodiscard]] virtual Result<void> DispatchCommand(std::string_view command_id, std::span<const u8> payload) = 0;
    virtual void Unload() noexcept = 0;
};

class RuntimeEngine {
public:
    virtual ~RuntimeEngine() = default;
    [[nodiscard]] virtual Result<scope<RuntimeInstance>> Create(const ExtensionPackage& package, host::HostApi host) = 0;
};

class Runtime final {
public:
    explicit Runtime(scope<RuntimeEngine> engine = {}) noexcept;
    ~Runtime();

    Runtime(const Runtime&) = delete;
    Runtime& operator=(const Runtime&) = delete;

    void SetEngine(scope<RuntimeEngine> engine) noexcept;
    void SetEventService(std::shared_ptr<host::EventService> service) noexcept;
    [[nodiscard]] Result<void> Load(const ExtensionPackage& package, EffectiveCapabilities grants);
    [[nodiscard]] Result<void> Load(const ExtensionPackage& package);
    void RecordFailure(std::string_view extension_id, const Error& error);
    void Tick(f64 delta_ms);
    void Tick(std::string_view extension_id, f64 delta_ms);
    void DispatchEvent(std::string_view extension_id, u32 event_type, std::span<const u8> payload);
    void DispatchNamedEvent(std::string_view extension_id, std::string_view topic, std::span<const u8> payload);
    [[nodiscard]] Result<void> DispatchCommand(std::string_view extension_id, std::string_view command_id, std::span<const u8> payload);
    void Unload(std::string_view extension_id) noexcept;
    void UnloadAll() noexcept;
    [[nodiscard]] bool IsActive(std::string_view extension_id) const noexcept;
    [[nodiscard]] bool IsSubscribed(std::string_view extension_id, u32 event_type) const noexcept;
    [[nodiscard]] bool IsSubscribed(std::string_view extension_id, std::string_view topic) const noexcept;
    [[nodiscard]] bool HasGrant(std::string_view extension_id, Permission permission) const noexcept;
    [[nodiscard]] std::span<const ExtensionStatus> Statuses() const noexcept;

private:
    struct Session;
    scope<RuntimeEngine> engine_;
    std::shared_ptr<host::EventService> event_service_;
    std::vector<scope<Session>> sessions_;
    std::vector<ExtensionStatus> statuses_;
};

} // namespace woki::ext
