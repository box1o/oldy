# Woki Extension SDK

Raw C ABI v1 is the authoritative contract used by the desktop Wasmtime backend
and the Emscripten web backend. Its memory, status, ownership, permission, and
limit semantics are normative in
[`../docs/raw-abi-v1.md`](../docs/raw-abi-v1.md).

The checked-in WIT files are a future Component Model v2 design. Current hosts
do not accept component binaries, and the files do not claim compatibility with
any current `wit-bindgen` release.

## Start a C++ extension

One source file can contain the entire plugin:

```cpp
#include <woki/ext/plugin.hpp>

using namespace woki::ext;

class Hello final : public Plugin {
public:
    Status OnLoad(Context& context) noexcept {
        const Status logged = context.GetLog().Info("hello from Woki");
        return logged ? context.GetEvents().Subscribe<WindowResizedEvent>() : logged;
    }

    void OnEvent(Event& event) noexcept {
        event.Dispatch<WindowResizedEvent>([](WindowResizedEvent resized) noexcept {
            (void)resized;
        });
    }
};

WOKI_PLUGIN(Hello)
```

Every callback is optional. Callbacks may omit `Context&`; `OnLoad` may return
`void`, `Status`, or a raw status code. Put `WOKI_PLUGIN(Hello)` in exactly one
translation unit. Other `.cpp` files may include `<woki/ext/plugin.hpp>`. The
plugin type must be trivially default-constructible and trivially destructible;
the macro enforces both so the freestanding guest needs no static-init guard or
destructor runtime.

Named topics remain separate from generated application events:

```cpp
Status OnLoad(Context& context) noexcept {
    return context.GetEvents().Subscribe("org.example.ready");
}

Status OnCommand(Context& context, StringView topic, Bytes payload) noexcept {
    return context.GetEvents().Emit(topic, payload);
}
```

The facade performs no allocation and requires neither exceptions nor RTTI.
Clang/clang++ targeting `wasm32` is the only supported guest compiler. Guest
headers reject wasm builds from GCC and other compilers. Bare Clang wasm targets
without C++ standard-library headers are supported.

## Guest headers

| Header | Purpose |
|--------|---------|
| `macros.h` | `WOKI_IMPORT` / `WOKI_EXPORT` for Clang wasm builds |
| `abi.h` | Aggregate raw ABI v1 constants and import module |
| `version.h` | `WOKI_EXT_API_VERSION` |
| `woki_limits.h` | Shared size limits for guest and host |
| `types.h` | Log levels and status codes |
| `<woki/ext/sdk/events.h>` | Stable event IDs, wire payload types, and allocation-free decoders |
| `host.h` | Host import API documentation |
| `host_imports.h` | Host imports with wasm import attributes |
| `ext.h` | Guest lifecycle exports |
| `guest_alloc.h` | Optional `ext_alloc` / `ext_free` buffer pool |

Guest exports:

- `ext_api_version`
- `ext_init`
- `ext_on_tick`
- `ext_on_event`
- `ext_on_event_named` optional callback for named topics
- `ext_on_unload`
- `ext_on_command` required only when the manifest contributes commands
- `ext_alloc` / `ext_free` required as a pair for manifests with the `events`
  permission or command contributions (include `guest_alloc.h`)

Verification checks exact import/export names and core-Wasm signatures,
requires one bounded exported linear memory, rejects start sections and
undeclared host capabilities, and always asks Wasmtime to standards-validate
the complete module when Wasmtime is available. It executes `ext_api_version`
during verification when no imports prevent isolated instantiation. Runtime
loading always checks the returned version exactly against the manifest.

Host import module:

```text
woki_host
```

Extensions should use `add_wokiext(<target> LANGUAGE <C|CXX> MANIFEST
<manifest> SOURCES <sources...> [ASSETS <assets...>])` from
`cmake/ExtensionWasm.cmake`. It accepts one or more same-language sources,
packages only the manifest, Wasm output, and allowlisted `assets/` files in the
binary tree, and exposes `WOKI_EXTENSION_WASM_OUTPUT` and
`WOKI_EXTENSION_PACKAGE_DIR` target properties. Configure it with upstream
Clang/clang++; no other wasm guest compiler is supported.

The raw C headers are installed below `<woki/ext/sdk/...>`. C++ guests need only
the allocation-free `<woki/ext/plugin.hpp>` facade. `WOKI_PLUGIN` detects
optional lifecycle methods and generates the raw exports and bounded allocator
pair. The C SDK remains available for direct ABI use.

Subscribe with a numeric application ID such as `WOKI_EXT_EVENT_WINDOW_RESIZED`, or call
`woki_ext_subscribe_all_events()` for wildcard delivery. Known application
event payloads use the fixed little-endian layouts documented in
[`../docs/raw-abi-v1.md`](../docs/raw-abi-v1.md); use the `woki_ext_decode_*`
helpers rather than parsing JSON.

Use `Events::Subscribe("org.example.extension.topic")` and
`Events::Emit(...)` for extension-defined events. These APIs preserve the exact
topic string and do not hash it into the numeric application-event namespace.
High-bit numeric extension IDs are deprecated compatibility-only IDs. Hosts
route them to matching numeric or wildcard subscribers that are already active,
but they never activate an extension.

Run the CI-safe contract check directly with:

```bash
python3 modules/extension/sdk/check_contract.py --cc clang --cxx clang++
```

Web support:

- Studio links `modules/extension/web/woki_ext.js` on Emscripten.
- The host reads `extension.wasm` once and gives the validated byte snapshot to the web backend.
- Source extensions under repo `extensions/` are preloaded at `/extensions` in
  web builds.
- Guest imports use the same permission model as native.
- Browser calls are synchronous and cannot interrupt an infinite loop.
  Web loading is disabled by default; `HostOptions::allow_trusted_synchronous_web`
  is an explicit opt-in for wasm that the application trusts to terminate.
