#pragma once

// Host implementation detail. Use CreateExtensionManager from the public API.

#include <memory>
#include <string>

#include "../runtime.hpp"

namespace woki::ext::wasm {

/// Wasm engine backed by the official Wasmtime C++ API (`wasmtime.hh`).
class WasmtimeEngine final : public RuntimeEngine {
public:
    WasmtimeEngine();
    ~WasmtimeEngine() override;

    WasmtimeEngine(const WasmtimeEngine&) = delete;
    WasmtimeEngine& operator=(const WasmtimeEngine&) = delete;

    [[nodiscard]] Result<scope<RuntimeInstance>> Create(const ExtensionPackage& package, host::HostApi host) override;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace woki::ext::wasm
