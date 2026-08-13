# Woki Extension SDK

`<woki/extension.hpp>` is the supported C++23 extension API. It is allocation-free,
requires neither RTTI nor exceptions, and compiles for `wasm32-unknown-unknown`.
The raw C ABI below `<woki/ext/sdk/>` is internal compatibility infrastructure for
the native Wasmtime and browser hosts.

## Extension class

```cpp
#include <woki/extension.hpp>

class Hello final {
public:
    woki::Status OnAttach() noexcept {
        return slog::Info("hello from Woki");
    }

    void OnUpdate(woki::f64 delta_ms) noexcept {}
    void OnEvent(woki::events::Event& event) noexcept {}

    woki::Status OnCommand(const woki::extension::Command& command) noexcept {
        return command.Id() == "org.example.run"
            ? slog::Info("running")
            : woki::Status::NotFound();
    }

    void OnDetach() noexcept {}
};

WOKI_EXTENSION(Hello)
```

Every callback is optional and must be `noexcept`. `OnAttach` and `OnCommand`
may return `void`, `woki::Status`, or `woki::i32`; the other callbacks return
`void`. The extension type may own non-trivial fixed-storage state. The SDK
constructs it before `OnAttach` and destroys it after `OnDetach`.

## Services

Services are global module-like APIs generated from `wit/host.wit` and lowered to
raw ABI v1 by the current hosts:

```cpp
woki::StringBuffer<4096> data_dir;
woki::paths::Data(data_dir);

woki::u8 bytes[1024];
woki::MutableBytes output{bytes, sizeof(bytes)};
woki::storage::Read("state.bin", output);
woki::storage::Write("state.bin", output.View());

woki::StringBuffer<256> value;
woki::config::Get("theme", value);
woki::config::Set("theme", "dark");

woki::events::Emit("org.example.ready");
```

Only services listed in manifest `permissions` can be linked or instantiated.
The `events` permission delivers every supported public application event.
Legacy numeric and named subscription imports remain validation-only no-ops and
are not exposed by the current C++ or WIT APIs.

## Guest libraries

Declare portable libraries in the manifest and include their module-like headers:

```yaml
libraries: [math, ecs]
```

```cpp
#include <woki/math/guest.hpp>
#include <woki/ecs/guest.hpp>
```

These libraries compile into the guest and do not provide host access. The guest
ECS uses fixed-capacity storage suitable for persistent extension members.

## Build

Use `wokiext create`, then `wokiext build`. CMake projects call:

```cmake
add_wokiext(extension LANGUAGE CXX MANIFEST manifest.yaml SOURCES src/extension.cpp)
```

The manifest controls runtime output, permissions, libraries, activation, and
commands. Packaging is transactional and records hashes of project, SDK, math,
and ECS inputs. Run the complete ABI/WIT check with:

```bash
python3 modules/extension/sdk/check_contract.py --cc clang --cxx clang++
```

Raw host-call ABI v1 details are documented in `docs/raw-abi-v1.md`; application
event payload ABI v2 is documented in `docs/application-events-v2.md`. Component Model
binaries are not yet accepted by the C/C++ Wasmtime embedding API used here;
the generated WIT adapter keeps the public service model independent of that
temporary lowering.
