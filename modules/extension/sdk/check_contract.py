#!/usr/bin/env python3
"""Static and compiler checks for the raw extension ABI v1 guest contract."""

from __future__ import annotations

import argparse
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path
from typing import NoReturn


SDK = Path(__file__).resolve().parent
EXTENSION = SDK.parent
ROOT = EXTENSION.parent.parent
FIXTURES = SDK / "fixtures"


def fail(message: str) -> NoReturn:
    raise RuntimeError(message)


def run(command: list[str], *, cwd: Path | None = None) -> None:
    completed = subprocess.run(command, cwd=cwd, text=True, capture_output=True)
    if completed.returncode != 0:
        detail = completed.stderr or completed.stdout
        fail(f"command failed: {' '.join(command)}\n{detail.strip()}")


def run_fails(command: list[str]) -> None:
    completed = subprocess.run(command, text=True, capture_output=True)
    if completed.returncode == 0:
        fail(f"command unexpectedly succeeded: {' '.join(command)}")


def block(text: str, name: str) -> str:
    match = re.search(
        rf"{re.escape(name)}:\s*Object\.freeze\(\{{(.*?)\}}\),", text, re.S
    )
    if not match:
        fail(f"missing JavaScript contract block {name}")
    return match.group(1)


def js_values(text: str, name: str) -> dict[str, int]:
    result: dict[str, int] = {}
    for key, expression in re.findall(
        r"^\s*(\w+):\s*([^,]+),", block(text, name), re.M
    ):
        if not re.fullmatch(r"[\d\s*<|()-]+", expression):
            fail(f"unsupported JavaScript constant expression: {expression}")
        result[key] = int(eval(expression, {"__builtins__": {}}, {}))
    return result


def c_defines(path: Path) -> dict[str, int]:
    expressions = dict(
        re.findall(r"^#define\s+(WOKI_EXT_\w+)\s+(.+)$", path.read_text(), re.M)
    )
    values: dict[str, int] = {}

    def resolve(name: str) -> int:
        if name in values:
            return values[name]
        expression = expressions[name].replace("u", "")
        for dependency in re.findall(r"WOKI_EXT_\w+", expression):
            expression = expression.replace(dependency, str(resolve(dependency)))
        if not re.fullmatch(r"[\d\s*<|()+-]+", expression):
            fail(f"unsupported C constant expression: {expressions[name]}")
        values[name] = int(eval(expression, {"__builtins__": {}}, {}))
        return values[name]

    for define in expressions:
        resolve(define)
    return values


def cmake_list(text: str, name: str) -> set[str]:
    match = re.search(rf"set\({name}\s+(.*?)\)", text, re.S)
    if not match:
        fail(f"missing CMake list {name}")
    return set(re.findall(r"\bext_[a-z_]+\b", match.group(1)))


def cmake_host_imports(text: str) -> set[str]:
    match = re.search(r"set\(WOKI_WASM_HOST_IMPORTS\s+(.*?)\)", text, re.S)
    if not match:
        fail("missing CMake host import allowlist")
    return set(re.findall(r"\bhost_[a-z_]+\b", match.group(1)))


def c_signatures(
    path: Path, attribute: str
) -> dict[str, tuple[tuple[str, ...], tuple[str, ...]]]:
    text = path.read_text()
    text = re.sub(r"//.*", "", text)
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.S)
    declarations = re.findall(
        rf'{attribute}\([^\n]*"((?:ext|host)_[a-z_]+)"\)\s*\n\s*([^;]+);', text
    )
    result: dict[str, tuple[tuple[str, ...], tuple[str, ...]]] = {}
    for exported_name, declaration in declarations:
        match = re.fullmatch(r"(.+?)\s+\w+\((.*)\)", declaration.strip())
        if not match:
            fail(f"cannot parse SDK declaration for {exported_name}: {declaration}")
        return_type, arguments = match.groups()

        def lower(c_type: str) -> str | None:
            c_type = re.sub(r"\b(?:const|volatile)\b", "", c_type)
            c_type = re.sub(r"\s+", " ", c_type).strip()
            if c_type == "void":
                return None
            if c_type == "double":
                return "F64"
            if "*" in c_type or c_type in {
                "uint32_t",
                "int32_t",
                "woki_ext_log_level_t",
                "woki_ext_event_type_t",
            }:
                return "I32"
            fail(f"unsupported SDK ABI type {c_type!r} in {exported_name}")

        parameters: list[str] = []
        if arguments.strip() != "void":
            for argument in arguments.split(","):
                argument_type = re.sub(r"\b\w+\s*$", "", argument.strip()).strip()
                lowered = lower(argument_type)
                if lowered:
                    parameters.append(lowered)
        lowered_result = lower(return_type)
        result[exported_name] = (
            tuple(parameters),
            (() if lowered_result is None else (lowered_result,)),
        )
    return result


def verifier_signature(parameters: tuple[str, ...], results: tuple[str, ...]) -> str:
    params = ", ".join(f"WasmValueType::{value}" for value in parameters)
    returns = ", ".join(f"WasmValueType::{value}" for value in results)
    return "{{" + params + "}, {" + returns + "}}"


def golden_signatures() -> tuple[
    dict[str, tuple[tuple[str, ...], tuple[str, ...]]],
    dict[str, tuple[tuple[str, ...], tuple[str, ...]]],
]:
    contract = json.loads((FIXTURES / "raw-abi-v1.json").read_text())

    def signatures(section: str) -> dict[str, tuple[tuple[str, ...], tuple[str, ...]]]:
        return {
            name: (tuple(value["parameters"]), tuple(value["results"]))
            for name, value in contract[section].items()
        }

    return signatures("exports"), signatures("imports")


def check_signatures() -> None:
    guest_expected, host_expected = golden_signatures()
    if c_signatures(SDK / "ext.h", "WOKI_EXPORT") != guest_expected:
        fail("SDK guest signatures do not match raw ABI v1")
    if c_signatures(SDK / "host.h", "WOKI_IMPORT") != host_expected:
        fail("SDK host signatures do not match raw ABI v1")

    verifier = (EXTENSION / "src" / "wasm" / "guest_module.cpp").read_text()
    for name, signature in guest_expected.items():
        expected = '{"' + name + '", ' + verifier_signature(*signature) + "}"
        if expected not in verifier:
            fail(f"guest verifier signature drift for {name}")
    for name, (parameters, results) in host_expected.items():
        if results != ("I32",):
            fail(f"checker assumes one i32 host result for {name}")
        params = ", ".join(f"WasmValueType::{value}" for value in parameters)
        if f'{{"{name}", {{Signature({{{params}}})' not in verifier:
            fail(f"host verifier signature drift for {name}")

    guest_wit = re.sub(r"\s+", " ", (EXTENSION / "wit" / "guest.wit").read_text())
    host_wit = re.sub(r"\s+", " ", (EXTENSION / "wit" / "host.wit").read_text())
    wit_declarations = {
        "api-version: func() -> u32;",
        "init: func() -> status;",
        "tick: func(delta-ms: f64);",
        "event: func(event-type: event-id, payload: list<u8>);",
        "unload: func();",
        "command: func(command-id: string, payload: list<u8>) -> status;",
        "log: func(level: level, message: string) -> status;",
        "data-dir: func() -> result<string, status>;",
        "cache-dir: func() -> result<string, status>;",
        "read: func(path: string) -> result<list<u8>, status>;",
        "write: func(path: string, data: list<u8>) -> status;",
        "append: func(path: string, data: list<u8>) -> status;",
        "get: func(key: string) -> result<string, status>;",
        "set: func(key: string, value: string) -> status;",
        "subscribe: func(event-type: event-id) -> status;",
        "emit: func(event-type: event-id, payload: list<u8>) -> status;",
    }
    combined_wit = guest_wit + " " + host_wit
    for declaration in wit_declarations:
        if declaration not in combined_wit:
            fail(f"WIT signature drift: {declaration}")


def check_static_contract() -> None:
    limits = c_defines(SDK / "woki_limits.h")
    permissions = c_defines(SDK / "perm_bits.h")
    types = (SDK / "types.h").read_text()
    event_schema = (SDK / "event_schema.def").read_text()
    statuses = {
        key: int(value)
        for key, value in re.findall(
            r"(WOKI_EXT_(?:OK|ERR|DENIED|NO_SPACE|NOT_FOUND|INVALID))\s*=\s*(-?\d+)",
            types,
        )
    }
    web = (EXTENSION / "web" / "woki_ext.js").read_text()

    sdk_events = {
        name: int(value)
        for name, value in re.findall(
            r"^WOKI_EXT_EVENT\([^,]+,\s*([A-Z_]+),\s*[^,]+,\s*(\d+),",
            event_schema,
            re.M,
        )
    }
    if len(sdk_events) != 22 or len(set(sdk_events.values())) != len(sdk_events):
        fail("event schema IDs must be complete and unique")

    expected_limits = {
        "log": limits["WOKI_EXT_MAX_LOG_LEN"],
        "configKey": limits["WOKI_EXT_MAX_CONFIG_KEY_LEN"],
        "configValue": limits["WOKI_EXT_MAX_CONFIG_VALUE_LEN"],
        "path": limits["WOKI_EXT_MAX_PATH_LEN"],
        "event": limits["WOKI_EXT_MAX_EVENT_LEN"],
        "eventTopic": limits["WOKI_EXT_MAX_EVENT_TOPIC_LEN"],
        "file": limits["WOKI_EXT_MAX_FILE_RW"],
        "memoryPages": limits["WOKI_EXT_MAX_MEMORY_PAGES"],
    }
    expected_status = {
        "ok": statuses["WOKI_EXT_OK"],
        "err": statuses["WOKI_EXT_ERR"],
        "denied": statuses["WOKI_EXT_DENIED"],
        "noSpace": statuses["WOKI_EXT_NO_SPACE"],
        "notFound": statuses["WOKI_EXT_NOT_FOUND"],
        "invalid": statuses["WOKI_EXT_INVALID"],
    }
    expected_permissions = {
        "log": permissions["WOKI_EXT_PERM_LOG"],
        "paths": permissions["WOKI_EXT_PERM_PATHS"],
        "storage": permissions["WOKI_EXT_PERM_STORAGE"],
        "config": permissions["WOKI_EXT_PERM_CONFIG"],
        "events": permissions["WOKI_EXT_PERM_EVENTS"],
    }
    for name, expected in (
        ("limits", expected_limits),
        ("status", expected_status),
        ("permission", expected_permissions),
    ):
        actual = js_values(web, name)
        if actual != expected:
            fail(f"web {name} drift: expected {expected}, got {actual}")

    required = {
        "ext_api_version",
        "ext_init",
        "ext_on_tick",
        "ext_on_event",
        "ext_on_unload",
    }
    optional = {"ext_on_command", "ext_on_event_named", "ext_alloc", "ext_free"}
    cmake = (ROOT / "cmake" / "ExtensionWasm.cmake").read_text()
    if cmake_list(cmake, "WOKI_WASM_GUEST_REQUIRED_EXPORTS") != required:
        fail("CMake required export set does not match raw ABI v1")
    if cmake_list(cmake, "WOKI_WASM_GUEST_OPTIONAL_EXPORTS") != optional:
        fail("CMake optional export set does not match raw ABI v1")

    ext_header = (SDK / "ext.h").read_text()
    header_exports = set(re.findall(r'WOKI_EXPORT\("(ext_[a-z_]+)"\)', ext_header))
    if header_exports != required | optional:
        fail("SDK export declarations do not match raw ABI v1")

    web_exports_match = re.search(
        r"requiredExports:\s*Object\.freeze\(\[(.*?)\]\)", web, re.S
    )
    if not web_exports_match:
        fail("missing web required export set")
    web_exports = set(re.findall(r"'(memory|ext_[a-z_]+)'", web_exports_match.group(1)))
    if web_exports != required | {"memory"}:
        fail("web required export set does not match raw ABI v1")

    host = (SDK / "host.h").read_text()
    declared_imports = set(
        re.findall(r'WOKI_IMPORT\(WOKI_EXT_IMPORT_MODULE, "(host_[a-z_]+)"\)', host)
    )
    expected_imports = {
        "host_log",
        "host_path_data",
        "host_path_cache",
        "host_file_read",
        "host_file_write",
        "host_file_append",
        "host_file_read_n",
        "host_file_write_n",
        "host_file_append_n",
        "host_config_get",
        "host_config_set",
        "host_event_subscribe",
        "host_event_emit",
        "host_event_subscribe_named",
        "host_event_emit_named",
    }
    if declared_imports != expected_imports:
        fail("host import declarations do not match raw ABI v1")
    if cmake_host_imports(cmake) != expected_imports:
        fail("CMake linker import allowlist does not match raw ABI v1")
    if "-Wl,--allow-undefined\n" in cmake:
        fail("CMake guest linker uses unrestricted --allow-undefined")

    worlds = (EXTENSION / "wit" / "world.wit").read_text()
    guest = (EXTENSION / "wit" / "guest.wit").read_text()
    if (
        "world extension-with-commands" not in worlds
        or "export commands;" not in worlds
    ):
        fail("WIT command world is missing")
    lifecycle = re.search(r"interface lifecycle\s*\{(.*?)\}", guest, re.S)
    if not lifecycle or "command:" in lifecycle.group(1):
        fail("WIT base lifecycle must not require commands")
    check_signatures()


def wasm_signatures(
    path: Path,
) -> tuple[
    dict[str, tuple[tuple[str, ...], tuple[str, ...]]],
    dict[str, tuple[tuple[str, ...], tuple[str, ...]]],
]:
    data = path.read_bytes()
    if data[:8] != b"\0asm\x01\0\0\0":
        fail("compiled ABI fixture is not a core wasm v1 module")
    offset = 8
    types: list[tuple[tuple[str, ...], tuple[str, ...]]] = []
    imported_types: list[int] = []
    defined_types: list[int] = []
    imports: dict[str, tuple[tuple[str, ...], tuple[str, ...]]] = {}
    exported_indices: dict[str, int] = {}
    value_types = {0x7F: "I32", 0x7E: "I64", 0x7D: "F32", 0x7C: "F64"}

    def u32(end: int) -> int:
        nonlocal offset
        value = 0
        shift = 0
        while offset < end and shift < 35:
            byte = data[offset]
            offset += 1
            value |= (byte & 0x7F) << shift
            if byte & 0x80 == 0:
                return value
            shift += 7
        fail("invalid ULEB128 in compiled ABI fixture")

    def name(end: int) -> str:
        nonlocal offset
        size = u32(end)
        if offset + size > end:
            fail("name exceeds compiled ABI fixture section")
        result = data[offset : offset + size].decode("utf-8")
        offset += size
        return result

    def values(end: int) -> tuple[str, ...]:
        nonlocal offset
        count = u32(end)
        result = []
        for _ in range(count):
            if offset >= end or data[offset] not in value_types:
                fail("unsupported value type in compiled ABI fixture")
            result.append(value_types[data[offset]])
            offset += 1
        return tuple(result)

    while offset < len(data):
        section_id = data[offset]
        offset += 1
        section_size = u32(len(data))
        end = offset + section_size
        if end > len(data):
            fail("section exceeds compiled ABI fixture")
        if section_id == 1:
            for _ in range(u32(end)):
                if offset >= end or data[offset] != 0x60:
                    fail("invalid function type in compiled ABI fixture")
                offset += 1
                types.append((values(end), values(end)))
        elif section_id == 2:
            for _ in range(u32(end)):
                module = name(end)
                imported_name = name(end)
                if offset >= end or data[offset] != 0:
                    fail("golden ABI fixture may only import functions")
                offset += 1
                type_index = u32(end)
                if type_index >= len(types):
                    fail("invalid imported function type in compiled ABI fixture")
                imported_types.append(type_index)
                if module == "woki_host":
                    imports[imported_name] = types[type_index]
        elif section_id == 3:
            defined_types.extend(u32(end) for _ in range(u32(end)))
        elif section_id == 7:
            for _ in range(u32(end)):
                exported_name = name(end)
                if offset >= end:
                    fail("truncated export in compiled ABI fixture")
                kind = data[offset]
                offset += 1
                index = u32(end)
                if kind == 0 and exported_name.startswith("ext_"):
                    exported_indices[exported_name] = index
        offset = end

    function_types = imported_types + defined_types
    exports = {}
    for exported_name, index in exported_indices.items():
        if index >= len(function_types) or function_types[index] >= len(types):
            fail(f"invalid function index for fixture export {exported_name}")
        exports[exported_name] = types[function_types[index]]
    return exports, imports


def check_compilers(cc: str, cxx: str) -> None:
    c_source = r"""
#include <woki/ext/sdk/abi.h>
#include <woki/ext/sdk/ext.h>
#include <woki/ext/sdk/host.h>
#include <woki/ext/sdk/host_imports.h>
#include <woki/ext/sdk/guest_alloc.h>
_Static_assert(WOKI_EXT_EVENT_WINDOW_RESIZED == 2u, "event id");
_Static_assert(WOKI_EXT_EVENT_EXTENSION_ID(42u) == 0x8000002au, "extension event id");
_Static_assert(sizeof(float) == 4u, "wire f32");
_Static_assert(WOKI_EXT_API_VERSION == 1u, "api version");
_Static_assert(WOKI_EXT_OK == 0 && WOKI_EXT_INVALID == -5, "status");
"""
    cxx_source = r"""
#include <woki/ext/plugin.hpp>
static_assert(WOKI_EXT_EVENT_APP_RESUME == 305u);
static_assert(WOKI_EXT_EVENT_EXTENSION_ID(42u) == 0x8000002au);
static_assert(WOKI_EXT_API_VERSION == 1u);
static_assert(WOKI_EXT_MAX_EVENT_LEN == WOKI_EXT_GUEST_BUFFER_SIZE);
static_assert(woki::ext::StringView("org.example.ready").Size() == 17u);
struct EmptyPlugin {};
struct OptionalPlugin {
    void OnLoad() {}
    void OnTick(double) {}
    void OnEvent(woki::ext::Event&) {}
    void OnUnload() {}
    int OnCommand(woki::ext::StringView, woki::ext::Bytes) { return WOKI_EXT_OK; }
};
void CheckOptionalCallbacks() {
    woki::ext::Context context;
    woki::ext::Event event{WOKI_EXT_EVENT_APP_RESUME, nullptr, 0u};
    EmptyPlugin empty;
    (void)woki::ext::detail::Load(empty, context);
    woki::ext::detail::Tick(empty, context, 1.0);
    woki::ext::detail::Deliver(empty, context, event);
    woki::ext::detail::Unload(empty, context);
    (void)woki::ext::detail::Command(empty, context, {}, {});
    OptionalPlugin optional;
    (void)woki::ext::detail::Load(optional, context);
    woki::ext::detail::Tick(optional, context, 1.0);
    woki::ext::detail::Deliver(optional, context, event);
    woki::ext::detail::Unload(optional, context);
    (void)woki::ext::detail::Command(optional, context, {}, {});
}
struct TestPlugin {
    woki::ext::Status OnLoad(woki::ext::Context&) { return woki::ext::Status::Success(); }
    void OnEvent(woki::ext::Event& event) {
        (void)event.Dispatch<woki::ext::MouseScrolledEvent>([](const auto& value) { return value.offset_y != 0.0f; });
    }
};
WOKI_PLUGIN(TestPlugin)
"""
    with tempfile.TemporaryDirectory(prefix="woki-contract-") as temporary:
        temp = Path(temporary)
        c_file = temp / "headers.c"
        cxx_file = temp / "headers.cpp"
        guest_file = FIXTURES / "raw-abi-v1-guest.c"
        freestanding = temp / "freestanding"
        freestanding.mkdir()
        (freestanding / "string.h").write_text(
            (FIXTURES / "freestanding" / "string.h.fixture").read_text()
        )
        second_file = temp / "second.c"
        second_cxx_file = temp / "second.cpp"
        nontrivial_cxx_file = temp / "nontrivial.cpp"
        c_file.write_text(c_source)
        cxx_file.write_text(cxx_source)
        second_file.write_text('#include "guest_alloc.h"\n')
        second_cxx_file.write_text(
            "#include <woki/ext/plugin.hpp>\n"
            'static_assert(woki::ext::StringView("second.tu").Size() == 9u);\n'
            "woki::ext::Status FromSecondTranslationUnit() { "
            "return woki::ext::Status::Success(); }\n"
        )
        nontrivial_cxx_file.write_text(
            "#include <woki/ext/plugin.hpp>\n"
            "struct NontrivialPlugin { NontrivialPlugin() {} };\n"
            "WOKI_PLUGIN(NontrivialPlugin)\n"
        )
        common = ["-I", str(SDK), "-Wall", "-Wextra", "-Werror", "-pedantic"]
        run([cc, "-std=c17", "-fsyntax-only", *common, str(c_file)])
        run([cxx, "-std=c++23", "-fsyntax-only", *common, str(cxx_file)])
        run_fails(
            [cxx, "-std=c++23", "-fsyntax-only", *common, str(nontrivial_cxx_file)]
        )
        run(
            [
                cxx,
                "--target=wasm32-unknown-unknown",
                "-std=c++23",
                "-nostdlib",
                "-fno-builtin",
                "-fno-exceptions",
                "-fno-rtti",
                "-fsyntax-only",
                *common,
                str(cxx_file),
            ]
        )
        object_common = ["--target=wasm32-unknown-unknown", "-nostdlib"]
        run(
            [
                cc,
                *object_common,
                "-std=c17",
                "-c",
                *common,
                str(c_file),
                "-o",
                str(temp / "one.o"),
            ]
        )
        run(
            [
                cc,
                *object_common,
                "-std=c17",
                "-c",
                *common,
                str(second_file),
                "-o",
                str(temp / "two.o"),
            ]
        )
        run(
            [
                cc,
                *object_common,
                "-r",
                str(temp / "one.o"),
                str(temp / "two.o"),
                "-o",
                str(temp / "combined.o"),
            ]
        )
        run(
            [
                cxx,
                *object_common,
                "-std=c++23",
                "-c",
                *common,
                str(cxx_file),
                "-o",
                str(temp / "one-cxx.o"),
            ]
        )
        run(
            [
                cxx,
                *object_common,
                "-std=c++23",
                "-c",
                *common,
                str(second_cxx_file),
                "-o",
                str(temp / "two-cxx.o"),
            ]
        )
        run(
            [
                cxx,
                *object_common,
                "-r",
                str(temp / "one-cxx.o"),
                str(temp / "two-cxx.o"),
                "-o",
                str(temp / "combined-cxx.o"),
            ]
        )
        expected_exports, expected_imports = golden_signatures()
        (temp / "host-imports.txt").write_text("\n".join(expected_imports) + "\n")
        run(
            [
                cc,
                "--target=wasm32-unknown-unknown",
                "-std=c17",
                "-nostdlib",
                "-fno-builtin",
                "-pipe",
                "-I",
                str(freestanding),
                "-I",
                str(SDK),
                "-Wl,--no-entry",
                f"-Wl,--allow-undefined-file={temp / 'host-imports.txt'}",
                "-Wl,--export-memory",
                "-Wl,--max-memory=33554432",
                "-Wl,--export=ext_api_version",
                "-Wl,--export=ext_init",
                "-Wl,--export=ext_on_tick",
                "-Wl,--export=ext_on_event",
                "-Wl,--export=ext_on_event_named",
                "-Wl,--export=ext_on_unload",
                "-Wl,--export=ext_on_command",
                "-Wl,--export=ext_alloc",
                "-Wl,--export=ext_free",
                str(guest_file),
                "-o",
                str(temp / "guest.wasm"),
            ]
        )
        actual_exports, actual_imports = wasm_signatures(temp / "guest.wasm")
        if actual_exports != expected_exports:
            fail(f"compiled guest export signature drift: {actual_exports}")
        if actual_imports != expected_imports:
            fail(f"compiled host import signature drift: {actual_imports}")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--cc", default="clang")
    parser.add_argument("--cxx", default="clang++")
    arguments = parser.parse_args()
    build_temp = ROOT / "build"
    if build_temp.is_dir():
        tempfile.tempdir = str(build_temp)
        os.environ["TMPDIR"] = str(build_temp)
    try:
        check_static_contract()
        check_compilers(arguments.cc, arguments.cxx)
        node = shutil.which("node")
        if node:
            run([node, "--check", str(EXTENSION / "web" / "woki_ext.js")])
    except (OSError, RuntimeError) as error:
        print(f"contract check failed: {error}", file=sys.stderr)
        return 1
    print("raw ABI v1 contract checks passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
