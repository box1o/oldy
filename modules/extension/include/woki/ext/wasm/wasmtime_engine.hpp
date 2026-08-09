#pragma once

// IWYU pragma: private, include "woki/ext/ext.hpp"

#include <memory>
#include <string>

#include "backend.hpp"

namespace woki::ext::wasm {

/// Wasm engine backed by the official Wasmtime C++ API (`wasmtime.hh`).
class WasmtimeEngine final : public Engine {
public:
    WasmtimeEngine();
    ~WasmtimeEngine() override;

    WasmtimeEngine(const WasmtimeEngine&) = delete;
    WasmtimeEngine& operator=(const WasmtimeEngine&) = delete;

    [[nodiscard]] Result<void> Load(Record& record, host::HostApi& host) override;
    [[nodiscard]] Result<u32> ApiVersion(Record& record) override;
    [[nodiscard]] Result<i32> Init(Record& record) override;
    [[nodiscard]] Result<void> Tick(Record& record, f64 delta_ms) override;
    [[nodiscard]] Result<void> Event(Record& record, u32 event_type, std::span<const u8> payload) override;
    [[nodiscard]] Result<i32> Command(Record& record, std::string_view command_id, std::span<const u8> payload) override;
    void Discard(Record& record) override;
    void Unload(Record& record) override;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace woki::ext::wasm
