# WIT Service Model

The checked-in WIT package `woki:extension@2.0.0` is the source model for the
public C++ host-service adapter. `sdk/generate_wit_bindings.py` validates the
interfaces and generates `sdk/woki/extension/detail/wit_host.hpp`. Contract tests
reject stale generated bindings.

Files:

- `wit/host.wit` defines logging, paths, storage, config, and event emission.
- `wit/guest.wit` defines lifecycle, command, and named-event callbacks.
- `wit/world.wit` composes the supported extension worlds.

Permission mapping:

| Manifest permission | WIT interface |
|---------------------|---------------|
| `log` | `logging` |
| `paths` | `paths` |
| `storage` | `storage` |
| `config` | `config` |
| `events` | `events` |

The current native and browser hosts lower these generated services to raw ABI
v1. Manifest permissions constrain both guest linking and host instantiation.
The `events` grant delivers every public application event and allows emission;
the legacy raw subscription functions are compatibility no-ops and are absent
from WIT.

Wasmtime's Component Model bindgen is currently a Rust API and is not exposed by
the C/C++ embedding API used by Woki. Consequently, hosts accept core Wasm v1
modules today. The WIT-first service boundary prevents that engine limitation
from leaking into extension source and provides the migration point for native
components when the embedding API supports them.

Studio creates an `ext::ExtensionManager` using `CreateExtensionManager`:

- native: `wasm::WasmtimeEngine`
- web: `wasm::WebEngine`

The web backend is synchronous and disabled by default for untrusted modules.
`HostOptions::allow_trusted_synchronous_web` is the explicit trusted-code opt-in.
