# Graphics Assets And Cameras

`woki::gfx` owns shader descriptors, source composition, offline validation/reflection, cooked payloads, runtime shader modules, reflected layout caches, dependency-driven reload coordination, and renderer-independent camera data. It does not own materials, pipelines, render scheduling, backend shader translation, ECS integration, or a global asset manager.

The module depends on core handles/hashes, asset VFS/products, platform file events, and RHI resource creation. JSON parsing uses pinned nlohmann/json. Native tooling can build pinned Tint from Dawn revision `9fd4a731474f66c1ce5465f9b13919ad7487315d`; Tint is private and excluded from Emscripten runtime builds.

Descriptor parsing, composition, compilation, product cooking, and candidate builds may run on worker threads when their supplied VFS/compiler implementations permit it. `ShaderLibrary`, `LayoutCache`, and reload publication are externally synchronized and normally used on the RHI owner thread. `BorrowedShader` retains an immutable generation containing both payload and module, so its accessors remain valid across publication and even after library destruction. The module cache also retains superseded modules because RHI has no proven submission-completion epoch.

Failures are returned or emitted as stable structured diagnostics. Reload is staged: a failed or stale candidate never replaces the last-known-good product. There is no fallback reflection when Tint is unavailable.

Descriptor entry points are authoritative: cooked interfaces contain only the declared stage/name pairs. Permutations are pipeline-constant identities backed by same-named WGSL overrides with validated scalar domains; they never rewrite source or alias static/skinned entry selection. The single-product CLI therefore refuses a multi-variant descriptor until an explicit variant is selected by a caller.

Tests cover JSONC descriptors, safe includes and source maps, deterministic variants/products/layouts, and stale/failed reload publication. Profile composition/compiler/cook separately from runtime module and layout cache misses; hashes make those boundaries observable without backend types.

The standard authored WGSL pack is installed at `share/woki/assets/shaders`. `standard.woki-shader-pack` is the source manifest and stable namespace; loading it parses and composes every descriptor and computes an aggregate hash over declared metadata, canonical dependency paths, and source content. WGSL validation is a separate, explicit Tint step and is never reported successful when Tint is absent. `ShaderPackIndex` is the deterministic cooked lookup representation. Emscripten applications may link `woki::gfx_asset_preload` to mount the staged pack at `/assets/shaders` without coupling gfx itself to an application target. `CreateShaderModuleFromAsset` is a development/demo source-loading path; production code should publish verified cooked products through `ShaderLibrary`.

Camera poses, projections, normalized viewports, derived views, and orbit/fly controllers are plain state with no ECS or platform dependency. Camera projections are explicitly right-handed WebGPU zero-to-one depth; the existing core OpenGL projection helpers retain their original convention. Physical lens fields are metadata except for vertical-FOV/focal-length conversion and do not imply depth-of-field rendering.
