# Future Component Model API

The checked-in WIT files are an exploratory design for a future Component Model
v2 API. They are not a supported guest interface, are not tested against a
particular `wit-bindgen` release, and do not describe binaries accepted by the
current hosts. [Raw ABI v1](raw-abi-v1.md) is the sole authoritative extension
binary contract.

Files:

- `wit/host.wit` defines host services available to extensions.
- `wit/guest.wit` defines lifecycle and conditional command callbacks.
- `wit/world.wit` composes the extension world.

The exploratory package is versioned as `woki:extension@2.0.0`. `extension` is
the base world. Additional worlds opt into command dispatch, named-event
delivery, or both. WIT has no optional function exports, so separate worlds
make those guest capabilities explicit.

Changes to the current extension contract must be made in raw ABI v1 first.
Keep WIT aligned when practical, without treating it as a compatibility claim.

Permission mapping:

| Manifest permission | WIT interface |
|---------------------|---------------|
| `log` | `logging` |
| `paths` | `paths` |
| `storage` | `storage` |
| `config` | `config` |
| `events` | `events` |

Manifest v1's `events` permission maps to both subscription/receive and emit;
the version has no way to grant those directions separately. Event type `0`
is an explicit wildcard, the default subscription set is empty, and guest
emissions use the high-bit namespace described by the raw ABI contract. New
extension-defined events instead use exact reverse-DNS topics through the named
subscribe, emit, and callback functions. Deprecated high-bit numeric IDs remain
for compatibility and reach matching subscribers only when already active;
they are not activation events.

Runtime backend:

Studio creates an `ext::ExtensionManager` with the public `CreateExtensionManager`
factory:

- native: `wasm::WasmtimeEngine`
- web: `wasm::WebEngine`

`WebEngine` uses the browser `WebAssembly` API through `web/woki_ext.js`. It
instantiates the host's validated byte snapshot, wires `woki_host`
imports, calls lifecycle exports, and forwards host storage to the Emscripten
filesystem under the extension data root. Browser wasm calls are synchronous
and cannot be interrupted, so loading is disabled by default. The explicit
`HostOptions::allow_trusted_synchronous_web` opt-in is for trusted, terminating wasm only; untrusted web
execution requires a future asynchronous Worker-backed runtime contract.
