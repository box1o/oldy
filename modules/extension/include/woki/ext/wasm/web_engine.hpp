#pragma once

// Host implementation detail. Use CreateExtensionManager from the public API.

#include "../runtime.hpp"

namespace woki::ext::wasm {

/// Engine implementation that runs wasm through browser WebAssembly on Emscripten.
///
/// Calls are synchronous and cannot interrupt a looping guest. See
/// docs/web-runtime-isolation.md for the Worker conversion boundary.
class WebEngine final : public RuntimeEngine {
public:
    explicit WebEngine(bool allow_trusted_synchronous_execution = false) noexcept;

    [[nodiscard]] Result<scope<RuntimeInstance>> Create(const ExtensionPackage& package, host::HostApi host) override;

private:
#ifdef __EMSCRIPTEN__
    bool allow_trusted_synchronous_execution_;
#endif
};

[[nodiscard]] scope<RuntimeEngine> CreateEngine(bool allow_trusted_synchronous_web = false);

} // namespace woki::ext::wasm
