# Asset Module

## Purpose

`woki::asset` owns stable identity, canonical virtual URIs, mounted storage,
source/product metadata, dependency invalidation, deterministic builders, WKAS
containers, the local product cache, and asynchronous generation publication.
`AssetId` is persisted identity; source/product hashes describe content; runtime
handles belong to consumers; `AssetVersion` describes a published generation.

It does not define mesh, texture, material, graphics-backend, or native watcher
implementations. Builders produce backend-neutral products.

## Threads And Lifetime

`Vfs` mount snapshots and `MemoryMount` support concurrent readers and updates.
Database transactions commit optimistically. `AssetManager` owns its bounded
scheduler, separate prefetch/normal/high I/O lanes, and completion queue. Visible
requests therefore do not queue behind prefetch work; duplicate requests coalesce
and record priority escalation. Jobs only create candidates;
callers invoke `PumpPublications` on the owner thread to mutate live records.
Leases retain immutable generations across reload. Destroy the manager after
request producers and before services captured by its loader.

## Failure And Reload

Boundary APIs return `Result`. Failed rebuilds retain the previous good
generation and expose the latest structured error. Invalidation increments a
revision and rejects stale publication. Reload uses content hashes and transitive
dependencies, never timestamps as identity. Watcher overflow calls `Reconcile`
for the affected mounted URI root.

## Cache And Products

WKAS v3 is explicitly little-endian, bounded, versioned, and trailing-byte
strict. Chunk records carry semantic, compression method, power-of-two
alignment, stored/decoded sizes, checksum, and sparse payload offset. Readers
reject overflow, overlap, unsupported compression, decompression-limit excess,
checksum mismatch, and bytes beyond the declared payload. Only `none` is
implemented; v1/v2 products are rebuild-only because semantic chunks cannot be
inferred losslessly.
Its canonical hash covers identity, subresource, versions, source hash, ordered
identity/product-hash dependencies, target fingerprint, chunks, and payload.
Cache paths include target, platform, capability fingerprint, asset type, and
product hash. Writes use a temporary file, per-product process lock directory,
and atomic rename; corrupt incumbents move to `quarantine/`. Runtime persistent
roots come from `RenderRuntimeDescriptor`, not the process working directory.

`AssetManifest` is the shipping lookup boundary. Runtime resolves `AssetId` to
an exact type/hash/version/target and `ProductLocator`; it does not scan cooked
directories. `ProductReader` reads WKAS metadata and selected chunks through VFS
or `ProductCache` ranges, so coarse mesh LOD and texture mip requests need not
read complete products. `BuildPackage` follows exact `ProductDependency`
reachability, rejects missing closure/hash mismatches/cycles, and emits a stable
bundle index ordered by `AssetId`. `woki-asset` exposes validate, build, inspect,
deps, upgrade, and package commands. Shipping package inputs are cooked products
only; authored sources and FBX files are never copied into web/install bundles.

## Profiling And Security

Profile queue wait, I/O, build, validation, publication, cache hits, resident
decoded bytes, and stale rejection separately. URI parsing rejects noncanonical
UTF-8, percent encoding, host paths, backslashes, NUL, and traversal. Directory
roots are trusted configuration, not hostile-root sandboxes. Manifest, product,
dependency, and payload counts are bounded before allocation.
