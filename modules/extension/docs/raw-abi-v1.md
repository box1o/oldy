# Raw extension ABI v1

The raw ABI is the compatibility contract used by the Wasmtime and web
backends. It is a core wasm ABI: values use wasm `i32`/`f64`, pointers are
unsigned 32-bit offsets into the guest's exported linear memory, and no
canonical ABI realloc or component-model lifting is involved. This document is
the authoritative extension binary contract. The checked-in WIT files are only
a future Component Model v2 design; generated component bindings are not the
binary interface accepted by the current backends.

Clang targeting `wasm32` is the only supported guest compiler for ABI v1. The
SDK headers reject wasm compilation by GCC or other compilers.

## Module and memory

Host functions are imported from `woki_host`. The guest must export `memory`.
Imports from other modules, unknown or misspelled `woki_host` imports, and ABI
signature mismatches are rejected during verification. `--allow-undefined` is
not an extension point for additional imports. Start sections are forbidden:
guest code may execute only through an explicit lifecycle callback.

The module must define exactly one memory with an explicit maximum no greater
than 512 pages (32 MiB); its minimum must also fit that cap. Unbounded memories
and larger declared maxima are rejected before instantiation. Guest linker
invocations must therefore include `--max-memory=33554432` (or the equivalent
toolchain option).
Every `(ptr, len)` pair denotes exactly `len` bytes and need not be NUL
terminated. A zero-length span may use pointer zero. All other pointers must be
in bounds for the duration of the call. Text is UTF-8; malformed UTF-8 handling
is backend-defined in ABI v1, so guests should emit valid UTF-8.

The legacy `host_file_read`, `host_file_write`, and `host_file_append` imports
take a NUL-terminated path. New guests should use the `_n` forms. Path and
config-key arguments must not contain embedded NUL bytes.

## Status

| Name | Value | Meaning |
|------|------:|---------|
| `WOKI_EXT_OK` | 0 | Success |
| `WOKI_EXT_ERR` | -1 | Unclassified host or guest failure |
| `WOKI_EXT_DENIED` | -2 | Required manifest permission is absent |
| `WOKI_EXT_NO_SPACE` | -3 | A limit or output capacity was exceeded |
| `WOKI_EXT_NOT_FOUND` | -4 | Requested file or config value is absent |
| `WOKI_EXT_INVALID` | -5 | Invalid pointer, path, key, or argument |

When more than one condition is invalid, the returned error is not guaranteed
to have a fixed precedence. Guest callbacks returning `int32_t` use zero for
success and a negative status for failure. For `ext_on_command`, documented
negative statuses become structured, nonfatal caller errors and do not unload
the guest instance. Traps and unknown status values remain fatal.

`host_file_read[_n]` treats `*inout_len` as output capacity. On success it is
the byte count written. When capacity is too small it is updated to the
required byte count and `WOKI_EXT_NO_SPACE` is returned.

## Exports and ownership

Always required:

- `uint32_t ext_api_version(void)` returns `WOKI_EXT_API_VERSION`.
- `int32_t ext_init(void)` initializes the instance once.
- `void ext_on_tick(double delta_ms)` receives elapsed milliseconds.
- `void ext_on_event(uint32_t type, const uint8_t *payload, uint32_t len)`.
- `void ext_on_unload(void)` runs before a normal unload.

Conditionally required:

- `ext_on_command` is required when the manifest contributes commands and
  otherwise optional.
- `ext_alloc` and `ext_free` are required as a pair when the manifest requests
  the `events` permission or contributes commands. Event guests use the pair
  for non-empty event payloads; command guests use it for the command ID and
  any non-empty command payload.

Every named ABI export must have exactly the signature shown by `sdk/ext.h`;
an export with the right name but wrong kind or signature is invalid. A
manifest with command contributions requires `ext_on_command`.

For inbound data, the host calls `ext_alloc(len)`, writes exactly `len` bytes,
calls the callback, then calls `ext_free(ptr, len)`. Ownership is temporary:
the guest must not retain the pointer after the callback, and `ext_free` must
accept the exact pointer/length pair returned by `ext_alloc`. The host never
calls either function for an empty span. An allocator must support at least two
simultaneous allocations because command ID and payload can coexist. The SDK
allocator supports this and coalesces definitions across translation units;
define `WOKI_EXT_GUEST_BUFFER_COUNT` before including `guest_alloc.h` to reserve
more slots.

An `ext_free` trap is a fatal callback failure. Hosts do not report a callback
as successful when cleanup failed, and guests must not use `ext_free` for
best-effort diagnostics or other fallible work.

Callbacks are serialized by the current hosts, but ABI v1 does not grant a
guest permission to call lifecycle exports recursively. Guest-to-host imports
may execute host code before returning, so allocator state must be committed
before returning a pointer.

## Permissions

| Manifest permission | Imports |
|---------------------|---------|
| `log` | `host_log` |
| `paths` | `host_path_data`, `host_path_cache` |
| `storage` | `host_file_read[_n]`, `host_file_write[_n]`, `host_file_append[_n]` |
| `config` | `host_config_get`, `host_config_set` |
| `events` | `host_event_subscribe`, `host_event_emit`, `host_event_subscribe_named`, `host_event_emit_named` |

Manifest v1 has one `events` permission, so it grants receipt of every ABI v1
public application event and allows event emission. A future manifest version
may split these grants; hosts must not infer that split in v1. The legacy
`host_event_subscribe` and `host_event_subscribe_named` imports remain
permission-checked compatibility no-ops. Their arguments are validated where
applicable, but they do not filter delivery.

Guest emissions must set bit 31
(`WOKI_EXT_EVENT_NAMESPACE`); use `WOKI_EXT_EVENT_EXTENSION_ID(local_id)` or
`woki_ext_extension_event_id(local_id)` to construct an ID. Type `0` and
host-space types are rejected. The
host copies and queues emitted payloads, then publishes them to its event bus
with extension origin metadata after the current guest callback returns.

New extension-defined events use exact named topics. The compatibility
`host_event_subscribe_named` call only validates a lowercase reverse-DNS topic;
`host_event_emit_named` emits a topic plus payload. Emitted topics must begin with the extension manifest id and
a dot, for example `org.example.tool.ready`. Topic names are at most 255 bytes;
empty names, malformed labels, non-ASCII bytes, and embedded NULs are rejected.
Hosts deliver named events through the optional `ext_on_event_named` export.
Numeric extension IDs remain available only for raw compatibility and should
not be used for new events because they cannot provide collision-free identity.
The Manager routes a high-bit numeric event to every already active session
with the effective `events` grant. Such events never activate a package.
Unknown host-space numeric IDs are ignored. Named events follow the same
active-session and effective-grant rule.

### Application events

Known application events are binary, not JSON. Their payload schema is versioned
independently from these raw host-call signatures. Current hosts emit
application-event ABI v2, documented in [application-events-v2.md](application-events-v2.md).

An inactive package requesting `events` is activated when the first supported
public application event arrives. The event is delivered after activation only
when capability policy grants `events`; all later public application events are
delivered under the same effective grant.

The stable IDs and generated decoders are defined by `sdk/event_schema.def` and
`sdk/events.h`. Legacy mouse and `KeyTyped` events are not part of v2.

Declare every imported capability in `manifest.yaml`. Verification rejects an
import when its corresponding permission is absent, consistently across the
native and web backends.

## Limits

Limits are byte counts and are canonical in `sdk/woki_limits.h`: logs 4096,
config keys 128, config values 64 KiB, paths 4096, event and command payloads
64 KiB, and each file operation/final file 16 MiB. Output strings include a
trailing NUL in the supplied capacity. Permission bits and statuses are
canonical in `sdk/perm_bits.h` and `sdk/types.h`; `sdk/abi.h` aggregates the raw
ABI constants. The linear-memory cap is 512 wasm pages (32 MiB).
