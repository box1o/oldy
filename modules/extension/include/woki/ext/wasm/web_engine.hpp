#pragma once

// IWYU pragma: private, include "woki/ext/ext.hpp"

#include "backend.hpp"

namespace woki::ext::wasm {

/// Engine implementation that runs wasm through browser WebAssembly on Emscripten.
///
/// This keeps `wasm::Backend` as the only runtime backend used by Studio while
/// allowing the actual engine to be swapped per platform.
class WebEngine final : public Engine {
public:
    [[nodiscard]] Result<void> Load(Record& record, host::HostApi& host) override;
    [[nodiscard]] Result<u32> ApiVersion(Record& record) override;
    [[nodiscard]] Result<i32> Init(Record& record) override;
    [[nodiscard]] Result<void> Tick(Record& record, f64 delta_ms) override;
    [[nodiscard]] Result<void> Event(Record& record, u32 event_type, std::span<const u8> payload) override;
    [[nodiscard]] Result<i32> Command(Record& record, std::string_view command_id, std::span<const u8> payload) override;
    void Discard(Record& record) override;
    void Unload(Record& record) override;
};

} // namespace woki::ext::wasm
