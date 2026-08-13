# GPU-driven rendering extension

The GPU-driven path is an advanced renderer implementation detail. Scene and asset handles remain logical CPU identities; `GpuResourceIndex` and dense GPU-scene indices are rebuildable physical references and are invalidated on device recovery.

## Frame flow

1. CPU visibility produces the deterministic candidate superset and existing `DrawPacket` fallback.
2. The previous frame's persistent `R32Float` Hi-Z pyramid is imported. Camera cuts, resize, format changes, and pipeline changes invalidate occlusion history.
3. Compute performs layer and conservative sphere-frustum tests, projected-error resident LOD selection, optional conservative Hi-Z tests, and meshlet sphere/cone tests.
4. Work is classified into bounded immutable pipeline/material/mesh bins. The graph exposes indexed commands, per-bin counts, visible-instance and meshlet lists, and overflow diagnostics as `IndirectDrawStream` resources.
5. Depth, opaque, and shadow passes use counted indexed indirect draws only when all programs, PSOs, GPU-scene tables, classic indexed streams, and RHI capabilities are ready. Any missing prerequisite selects the unchanged CPU packet path.

Hi-Z and visibility compute passes request the compute queue as an advisory preference. The graph may collapse them onto graphics, including on WebGPU. Timestamp profiling, not queue labels, determines whether overlap is useful.

## Correctness and capacity

Occlusion is disabled for invalid history and validation mode. Bounds and mip selection are conservative; uncertain work remains visible. Capacities are fixed per feature configuration. Overflow is counted and asynchronously read back, reported as `RND3101`, and selects deterministic CPU fallback. Validation asynchronously compares dense visible object sets and reports only missing GPU objects as `RND3102`; GPU false positives are permitted.

`BuildPsoWarmupManifest` deterministically derives warmup entries from reachable material/pipeline/bin combinations. Driver caches may accelerate creation but are never treated as the manifest or source of truth. Mesh products retain classic indexed LODs and additionally expose uploaded meshlet descriptor, bounds, vertex-index, and triangle streams.

## Capability fallbacks

GPU submission requires compute, storage buffers/textures, indirect draw, counted multi-draw, compute-written indirect support, `R32Float` sampled/storage support, cooked Hi-Z/visibility programs, valid GPU-scene tables, resident static mesh streams, and ready material PSOs. Skinned meshes, unsupported material vertex programs, pending PSOs, invalid history, and capacity failures remain on CPU packets. Compatibility pipelines omit the extension entirely.
