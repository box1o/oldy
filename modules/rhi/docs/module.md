# RHI

The RHI exposes backend-neutral resources and an immutable `DeviceCapabilities` snapshot. Code above this module selects formats, queue work, fallbacks, compression, timestamp queries, indirect paths, storage/readback paths, and presentation/HDR paths from that snapshot. Backend names are diagnostic data only.

Indirect argument records and count records have typed, WebGPU-compatible RHI layouts. `GpuDrivenIndirect` distinguishes compute-written counted streams from basic CPU-authored indirect draws; WebGPU conversion remains private to the backend, while Null and Validation preserve command diagnostics.

## Backends

- WebGPU populates normalized limits and features conservatively. Core format support is explicit; optional feature support is never inferred from adapter names.
- NullRHI is selected with `BackendType::Null` or created with `CreateNullInstance`. It stores logical buffer and texture bytes, executes copies and clears, logs draw/dispatch commands symbolically, and advances deterministic submission epochs. It does not rasterize pixels.
- ValidationRHI decorates any device with `CreateValidationDevice`. Diagnostics use stable `RHI-VAL-NNN` codes and retain recent command breadcrumbs. Standard mode checks descriptors, capability limits, ownership, ranges, copy bounds, query legality, and encoder/pass state. Full mode is reserved for additional expensive lifetime tracking.

`RunConformance` is the shared NullRHI/WebGPU smoke API. It covers buffer write/copy/map/readback, texture upload/copy, render clear/draw legality, compute dispatch, and submission completion.

## Device Loss

`Device::IsLost` and `Device::LossReason` are backend-neutral polling hooks. RenderRuntime combines them with asynchronous loss callbacks and enters recovery before accepting more submissions.

## Native Interop

New tool/backend interop should include `woki/rhi/advanced.hpp`. Existing virtual `GetNativeHandles` methods remain temporarily in ordinary interfaces because removing them is an ABI-wide migration affecting all backend objects and existing test doubles. GFX and ordinary runtime paths do not require native handles.
