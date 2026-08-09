#!/usr/bin/env python3

import argparse
import re
import subprocess
import sys
from pathlib import Path


SUFFIXES = {".c", ".cc", ".cpp", ".cxx", ".h", ".hh", ".hpp", ".hxx"}
EXCLUDED_FILES = {"window_icon_data.hpp"}
INCLUDE = re.compile(r'^(\s*#\s*include\s*)([<"])([^>"]+)([>"])(.*)$')


def sources(entries: list[str]) -> list[Path]:
    files: set[Path] = set()
    for entry in entries:
        path = Path(entry)
        if path.is_file() and path.suffix in SUFFIXES and path.name not in EXCLUDED_FILES:
            files.add(path)
        elif path.is_dir():
            files.update(candidate for candidate in path.rglob("*") if candidate.is_file() and candidate.suffix in SUFFIXES and candidate.name not in EXCLUDED_FILES)
    return sorted(files)


def include_key(line: str) -> tuple[int, int, str]:
    match = INCLUDE.match(line)
    if match is None:
        raise ValueError(line)
    opener, name = match.group(2), match.group(3)
    group = 1 if name.startswith("woki/") else 0 if opener == "<" else 2
    return group, len(name), name.casefold()


def sort_includes(text: str) -> str:
    lines = text.splitlines(keepends=True)
    output: list[str] = []
    index = 0
    while index < len(lines):
        if INCLUDE.match(lines[index]) is None:
            output.append(lines[index])
            index += 1
            continue

        end = index
        includes: list[str] = []
        while end < len(lines) and (INCLUDE.match(lines[end]) is not None or lines[end].strip() == ""):
            if INCLUDE.match(lines[end]) is not None:
                includes.append(lines[end])
            end += 1

        groups: dict[int, list[str]] = {0: [], 1: [], 2: []}
        for include in includes:
            groups[include_key(include)[0]].append(include)

        ordered = []
        for group in groups.values():
            if group:
                if ordered:
                    ordered.append("\n")
                ordered.extend(sorted(group, key=include_key))
        if end > index and lines[end - 1].strip() == "":
            ordered.append("\n")

        output.extend(ordered)
        index = end

    return "".join(output)


def clang_format(text: str, path: Path) -> str:
    result = subprocess.run(["clang-format", "--style=file", f"--assume-filename={path}"], input=text, check=True, capture_output=True, text=True)
    return result.stdout


def formatted(path: Path) -> str:
    result = clang_format(path.read_text(), path)
    return clang_format(sort_includes(result), path)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--check", action="store_true")
    parser.add_argument("paths", nargs="*", default=["modules", "studio"])
    args = parser.parse_args()

    changed = []
    for path in sources(args.paths):
        result = formatted(path)
        current = path.read_text()
        if result == current:
            continue
        changed.append(path)
        if not args.check:
            path.write_text(result)

    if args.check and changed:
        for path in changed:
            print(path)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
