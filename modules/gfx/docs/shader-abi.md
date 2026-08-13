# Shader semantics and generated ABI

`.woki-shader` v2 descriptors may assign stable semantic identifiers with `bindings` (`"group:binding": "Name"`), `groups`, and an optional `semantic` on each selected entry point. Coordinates, resource types, visibility, and layouts always come from Tint; annotations referring to resources outside the selected-entry union are errors. Version-1 descriptors migrate in memory with empty semantics.

`ShaderInterface::hash` covers selected entries, semantic names, per-entry binding visibility, reflected resource facts, buffer layout metadata, overrides, and capabilities. `DiffInterfaces` returns path-based changes and `RequireInterface` rejects an unexpected hash or interface. Shader payload v3 parsing is strict; older cooked products must be recooked.

Each `.woki-gpu-struct` is a strict version-1 JSONC record. Supported host types are `f32`, `i32`, `u32`, vectors, matrices, fixed arrays, earlier nested `ref` records, and explicit byte `padding`. `bool` is rejected and must be encoded as `u32`. `woki-gpu-struct` applies WGSL host-shareable uniform/storage alignment, size, and stride rules and emits C++ offset/size assertions and WGSL declarations from the same inputs.

Generated files live under the build tree in `generated/include/woki/gfx/generated` and `generated/shaders/generated`. They are build/install artifacts and must not be edited. Standard shader cooking reads staged assets so `render_abi.wgsl` is a tracked input, then emits a minimal per-shader interface header before writing the cooked product.

Cross/Emscripten configurations do not run host tools. Point `WOKI_GFX_PREGENERATED_ABI_DIR` at the `generated` artifact root from a native tools build; configuration fails rather than compiling against stale or guessed layouts when it is absent.

Useful commands:

```text
woki-shader interface <descriptor> [asset-root]
woki-shader bindings <descriptor> [asset-root]
woki-shader generate-interface <descriptor> <header> [asset-root]
woki-shader diff-interface <expected-product> <actual-product>
woki-gpu-struct generate <header> <wgsl> <schema>...
```
