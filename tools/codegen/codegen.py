"""Toybox attribute codegen CLI.

Scans annotated headers under one or more roots and emits the aggregated reflection/serialization
registration into the build folder. Invoked by cmake/tbx_codegen.cmake; run --self-test standalone.

  python codegen.py --input-root <engine/include> --out-dir <build/msvc/generated> \
      --clang-arg=-I<engine/include> [--clang-arg=-I<backend dir> ...]
  python codegen.py --self-test
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

import clang.cindex as cx

import emit
import parse
from model import CodegenError

# A cheap text pre-filter so we only hand libclang the headers that actually carry markers.
_MARKERS = ("TBX_SERIALIZABLE", "TBX_EXPOSED_TO_SCRIPTING")


def discover_headers(roots: list[str]) -> list[str]:
    found: set[str] = set()
    for root in roots:
        for path in Path(root).rglob("*.h"):
            try:
                text = path.read_text(encoding="utf-8", errors="ignore")
            except OSError:
                continue
            if any(marker in text for marker in _MARKERS):
                found.add(str(path))
    return sorted(found)


def run(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Toybox attribute codegen")
    parser.add_argument("--input-root", action="append", default=[], help="header root(s) to scan")
    parser.add_argument("--out-dir", help="directory to emit generated files into")
    parser.add_argument("--clang-arg", action="append", default=[], help="extra clang arg (repeatable)")
    parser.add_argument("--libclang", help="explicit path to the libclang shared library")
    parser.add_argument("--self-test", action="store_true", help="run the golden-output unit tests")
    args = parser.parse_args(argv)

    if args.self_test:
        import tests

        return tests.run()

    if not args.out_dir:
        parser.error("--out-dir is required")
    if args.libclang:
        cx.Config.set_library_file(args.libclang)

    clang_args = parse.default_clang_args(args.clang_arg)
    headers = discover_headers(args.input_root)
    try:
        module = parse.parse_amalgam(headers, clang_args)
    except CodegenError as error:
        print(f"codegen error: {error}", file=sys.stderr)
        return 1

    for path in emit.write_reflection(module.types, args.out_dir):
        print(path)
    print(emit.write_luau_defs(module.types, module.enums, args.out_dir))
    print(emit.write_luau_functions(module.functions, args.out_dir))
    return 0


if __name__ == "__main__":
    sys.exit(run())
