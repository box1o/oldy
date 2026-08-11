#!/usr/bin/env python3
"""Generate the allocation-free C++ lowering for the Woki host WIT services."""

from __future__ import annotations

import argparse
import re
from pathlib import Path

SDK = Path(__file__).resolve().parent
HOST_WIT = SDK.parent / "wit" / "host.wit"
OUTPUT = SDK / "woki" / "extension" / "detail" / "wit_host.hpp"

EXPECTED_FUNCTIONS = {
    "types": set(),
    "logging": {"log: func(level: level, message: string) -> status;"},
    "paths": {
        "data-dir: func() -> result<string, status>;",
        "cache-dir: func() -> result<string, status>;",
    },
    "storage": {
        "read: func(path: string) -> result<list<u8>, status>;",
        "write: func(path: string, data: list<u8>) -> status;",
        "append: func(path: string, data: list<u8>) -> status;",
    },
    "config": {
        "get: func(key: string) -> result<string, status>;",
        "set: func(key: string, value: string) -> status;",
    },
    "events": {
        "emit: func(event-type: event-id, payload: list<u8>) -> status;",
        "emit-named: func(topic: string, payload: list<u8>) -> status;",
    },
}

HEADER = """#pragma once

// Generated from modules/extension/wit/host.wit. Do not edit directly.
#include <woki/extension/types.hpp>

namespace woki::extension::detail::wit {

inline Status Log(u32 level, StringView message) noexcept {
    return Status{host_log(level, message.Data(), message.Size())};
}

inline Status DataDir(char* out, u32 capacity) noexcept {
    return Status{host_path_data(out, capacity)};
}

inline Status CacheDir(char* out, u32 capacity) noexcept {
    return Status{host_path_cache(out, capacity)};
}

inline Status Read(StringView path, MutableBytes& out) noexcept {
    u32 size = out.Capacity();
    const Status status{host_file_read_n(path.Data(), path.Size(), out.Data(), &size)};
    out.SetSize(status.Ok() || status.Code() == WOKI_EXT_NO_SPACE ? size : 0u);
    return status;
}

inline Status Write(StringView path, Bytes data) noexcept {
    return Status{host_file_write_n(path.Data(), path.Size(), data.Data(), data.Size())};
}

inline Status Append(StringView path, Bytes data) noexcept {
    return Status{host_file_append_n(path.Data(), path.Size(), data.Data(), data.Size())};
}

inline Status ConfigGet(StringView key, char* out, u32 capacity) noexcept {
    if (key.Empty() || key.Data() == nullptr || key.Size() > WOKI_EXT_MAX_CONFIG_KEY_LEN)
        return Status::Invalid();
    char terminated[WOKI_EXT_MAX_CONFIG_KEY_LEN + 1u]{};
    for (u32 index = 0; index < key.Size(); ++index) {
        if (key.Data()[index] == 0)
            return Status::Invalid();
        terminated[index] = key.Data()[index];
    }
    return Status{host_config_get(terminated, out, capacity)};
}

inline Status ConfigSet(StringView key, StringView value) noexcept {
    if (key.Empty() || key.Data() == nullptr || key.Size() > WOKI_EXT_MAX_CONFIG_KEY_LEN)
        return Status::Invalid();
    char terminated[WOKI_EXT_MAX_CONFIG_KEY_LEN + 1u]{};
    for (u32 index = 0; index < key.Size(); ++index) {
        if (key.Data()[index] == 0)
            return Status::Invalid();
        terminated[index] = key.Data()[index];
    }
    return Status{host_config_set(terminated, value.Data(), value.Size())};
}

inline Status Emit(u32 type, Bytes payload) noexcept {
    return Status{host_event_emit(type, payload.Data(), payload.Size())};
}

inline Status EmitNamed(StringView topic, Bytes payload) noexcept {
    return Status{host_event_emit_named(topic.Data(), topic.Size(), payload.Data(), payload.Size())};
}

} // namespace woki::extension::detail::wit
"""


def parse_interfaces(wit: str) -> dict[str, str]:
    interfaces: dict[str, str] = {}
    for match in re.finditer(r"interface ([a-z-]+) \{", wit):
        depth = 1
        index = match.end()
        while index < len(wit) and depth:
            if wit[index] == "{":
                depth += 1
            elif wit[index] == "}":
                depth -= 1
            index += 1
        if depth:
            raise SystemExit(f"unterminated WIT interface {match.group(1)}")
        interfaces[match.group(1)] = wit[match.end() : index - 1]
    return interfaces


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()

    source = re.sub(r"///.*", "", HOST_WIT.read_text())
    wit = " ".join(source.split())
    if not wit.startswith("package woki:extension@2.0.0;"):
        raise SystemExit("host.wit package identity changed")
    interfaces = parse_interfaces(wit)
    if set(interfaces) != set(EXPECTED_FUNCTIONS):
        raise SystemExit(f"host.wit interface set changed: {set(interfaces)}")
    for name, body in interfaces.items():
        functions = {
            " ".join(match.split())
            for match in re.findall(
                r"[a-z][a-z0-9-]*:\s*func\([^;]*?\)(?:\s*->\s*[^;]+)?;",
                body,
            )
        }
        if functions != EXPECTED_FUNCTIONS[name]:
            raise SystemExit(f"host.wit {name} functions changed: {functions}")
    if "type status = s32;" not in interfaces["types"]:
        raise SystemExit("host.wit status must remain s32")
    if not re.search(
        r"enum level \{ debug, info, warn, error, \}", interfaces["logging"]
    ):
        raise SystemExit("host.wit logging level layout changed")
    if "subscribe:" in wit or "subscribe-named:" in wit:
        raise SystemExit(
            "subscription compatibility functions must not be exposed by WIT"
        )

    if args.check:
        if not OUTPUT.exists() or OUTPUT.read_text() != HEADER:
            raise SystemExit(
                "generated WIT host binding is stale; run generate_wit_bindings.py"
            )
    else:
        OUTPUT.write_text(HEADER)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
