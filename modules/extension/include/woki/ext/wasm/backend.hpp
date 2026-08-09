#pragma once

// IWYU pragma: private, include "woki/ext/ext.hpp"

#include <span>
#include <string_view>

#include "../runtime.hpp"
#include "../host/api.hpp"

namespace woki::ext::wasm {

class Engine {
public:
    virtual ~Engine() = default;

    [[nodiscard]] virtual Result<void> Load(Record& record, host::HostApi& host) = 0;
    [[nodiscard]] virtual Result<u32> ApiVersion(Record& record) = 0;
    [[nodiscard]] virtual Result<i32> Init(Record& record) = 0;
    [[nodiscard]] virtual Result<void> Tick(Record& record, f64 delta_ms) = 0;
    [[nodiscard]] virtual Result<void> Event(Record& record, u32 event_type, std::span<const u8> payload) = 0;
    [[nodiscard]] virtual Result<i32> Command(Record& record, std::string_view command_id, std::span<const u8> payload) = 0;

    virtual void Discard(Record& record) {
        (void)record;
    }

    virtual void Unload(Record& record) = 0;
};

class Backend final : public RuntimeBackend {
public:
    explicit Backend(scope<Engine> engine) noexcept;

    [[nodiscard]] static scope<Backend> Create();

    [[nodiscard]] Result<void> Load(Record& record) override;
    [[nodiscard]] Result<void> Initialize(Record& record) override;
    void Tick(Record& record, f64 delta_ms) override;
    void DispatchEvent(Record& record, u32 event_type, std::span<const u8> payload) override;
    [[nodiscard]] Result<void> DispatchCommand(Record& record, std::string_view command_id, std::span<const u8> payload) override;
    void Unload(Record& record) override;

private:
    scope<Engine> engine_;
};

} // namespace woki::ext::wasm
