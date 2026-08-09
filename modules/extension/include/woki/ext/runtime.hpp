#pragma once

// IWYU pragma: private, include "woki/ext/ext.hpp"

#include <span>
#include <memory>
#include <string_view>

#include "registry.hpp"

namespace woki::ext {

class RuntimeBackend {
public:
    virtual ~RuntimeBackend() = default;

    [[nodiscard]] virtual Result<void> Load(Record& record) = 0;
    [[nodiscard]] virtual Result<void> Initialize(Record& record) = 0;
    virtual void Tick(Record& record, f64 delta_ms) = 0;
    virtual void DispatchEvent(Record& record, u32 event_type, std::span<const u8> payload) = 0;
    [[nodiscard]] virtual Result<void> DispatchCommand(Record& record, std::string_view command_id, std::span<const u8> payload) = 0;
    virtual void Unload(Record& record) = 0;
};

class Runtime final {
public:
    explicit Runtime(RuntimeBackend* backend = nullptr) noexcept;
    explicit Runtime(scope<RuntimeBackend> backend) noexcept;

    void SetBackend(RuntimeBackend* backend) noexcept;
    void SetBackend(scope<RuntimeBackend> backend) noexcept;
    [[nodiscard]] RuntimeBackend* Backend() noexcept;
    [[nodiscard]] const RuntimeBackend* Backend() const noexcept;

    [[nodiscard]] Result<void> Load(Record& record);
    [[nodiscard]] Result<void> Initialize(Record& record);
    void Tick(Record& record, f64 delta_ms);
    void DispatchEvent(Record& record, u32 event_type, std::span<const u8> payload);
    [[nodiscard]] Result<void> DispatchCommand(Record& record, std::string_view command_id, std::span<const u8> payload);
    void Unload(Record& record);

private:
    scope<RuntimeBackend> owned_backend_;
    RuntimeBackend* backend_{nullptr};
};

} // namespace woki::ext
