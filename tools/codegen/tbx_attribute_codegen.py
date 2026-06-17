#!/usr/bin/env python3
from __future__ import annotations

import argparse
import sys
import unittest
from pathlib import Path

from generator import resolve_include_path, run_codegen
from model import CodegenError
from parser import parse_source
from tests import AttributeCodegenTests


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description="Generate Toybox reflection glue from [[tbx::*]] attributes.")
    parser.add_argument("--input", type=Path)
    parser.add_argument("--output", type=Path)
    parser.add_argument("--output-dir", type=Path)
    parser.add_argument("--output-header", type=Path)
    parser.add_argument("--output-source", type=Path)
    parser.add_argument("--output-plugin-meta", type=Path)
    parser.add_argument("--include-root", type=Path)
    parser.add_argument("--plugin-abi-version", default="1")
    parser.add_argument("--plugin-resource-directory")
    parser.add_argument("--script-input", type=Path, action="append", default=[])
    parser.add_argument("--script-include-root", type=Path)
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args(argv)

    if args.self_test:
        suite = unittest.defaultTestLoader.loadTestsFromTestCase(AttributeCodegenTests)
        result = unittest.TextTestRunner(verbosity=2).run(suite)
        return 0 if result.wasSuccessful() else 1

    if args.input is None:
        parser.error("--input is required unless --self-test is used")

    output_header = args.output_header or args.output
    output_source = args.output_source
    if args.output_dir is not None:
        output_header = args.output_dir / f"{args.input.stem}.generated.h"
        output_source = args.output_dir / f"{args.input.stem}.generated.cpp"

    if output_header is None:
        parser.error("--output, --output-header, or --output-dir is required unless --self-test is used")

    if output_source is None:
        source_name = output_header.name
        if source_name.endswith(".generated.h"):
            source_name = source_name.removesuffix(".generated.h") + ".generated.cpp"
        else:
            source_name = output_header.stem + ".generated.cpp"
        output_source = output_header.with_name(source_name)

    try:
        script_types = []
        script_include_paths = []
        for script_input in args.script_input:
            script_types.extend(parse_source(script_input.read_text(encoding="utf-8"), str(script_input)))
            script_include_paths.append(resolve_include_path(script_input, args.script_include_root))

        run_codegen(
            args.input,
            output_header,
            output_source,
            args.include_root,
            args.plugin_abi_version,
            script_types,
            script_include_paths,
            args.output_plugin_meta,
            args.plugin_resource_directory,
        )
    except CodegenError as error:
        print(f"tbx_attribute_codegen: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
