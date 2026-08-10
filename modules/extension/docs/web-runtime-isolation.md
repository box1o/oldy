# Web runtime isolation

The native runtime bounds every guest call with Wasmtime fuel and per-store resource limits. Browser `WebAssembly.Instance` calls are synchronous and cannot be interrupted by a timer on the browser main thread. The current synchronous `RuntimeInstance` API therefore cannot honestly provide an infinite-loop timeout for `WebEngine`. A malicious or merely broken guest can permanently block the browser UI thread.

The web bridge is deliberately bounded behind opaque, process-unique session handles. Extension IDs are labels only and are never used as the JavaScript instance-map key. This keeps sessions isolated from global ID collisions and makes the bridge suitable for a future Worker transport without changing guest identity semantics.

Full loop isolation requires moving module creation and every lifecycle call into a dedicated Worker. That conversion also requires an asynchronous runtime contract for initialization, tick, event, command, and unload results. Until that API exists, web calls remain synchronous and no timeout is claimed or simulated.

`WebEngine` therefore refuses to load by default. `HostOptions::allow_trusted_synchronous_web`
(`WebEngine(true)` internally) is the
explicit synchronous-execution opt-in and is suitable only when the embedding
application has independently established that the wasm is trusted to
terminate. The default public factory options do not opt in. Untrusted extension loading must
remain disabled until lifecycle execution is moved to Workers behind an
asynchronous API.
