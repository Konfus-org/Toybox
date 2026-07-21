#!/usr/bin/env python3
from __future__ import annotations

import argparse
import sys
import unittest
from pathlib import Path

from generator import (
    registrar_call_lines,
    resolve_include_path,
    run_codegen,
    run_module_registration_codegen,
)
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
    # A plugin/app entry point calls the registrars of EVERY attribute-bearing header in its module;
    # --script-input is the historical spelling kept as an alias of --registration-input.
    parser.add_argument(
        "--registration-input",
        "--script-input",
        dest="registration_input",
        type=Path,
        action="append",
        default=[],
    )
    parser.add_argument(
        "--registration-include-root",
        "--script-include-root",
        dest="registration_include_root",
        type=Path,
    )
    parser.add_argument("--emit-module-registration", action="store_true")
    parser.add_argument("--module-name")
    parser.add_argument("--module-api-macro", default="")
    parser.add_argument("--module-output", type=Path)
    parser.add_argument("inputs", nargs="*", type=Path)
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args(argv)

    if args.self_test:
        suite = unittest.defaultTestLoader.loadTestsFromTestCase(AttributeCodegenTests)
        result = unittest.TextTestRunner(verbosity=2).run(suite)
        return 0 if result.wasSuccessful() else 1

    if args.emit_module_registration:
        if args.module_name is None or args.module_output is None:
            parser.error("--emit-module-registration requires --module-name and --module-output")
        try:
            run_module_registration_codegen(
                args.module_name,
                args.module_api_macro,
                args.module_output,
                args.inputs,
                args.include_root,
            )
        except CodegenError as error:
            print(f"tbx_attribute_codegen: {error}", file=sys.stderr)
            return 1
        return 0

    if args.input is None:
        parser.error("--input is required unless --self-test or --emit-module-registration is used")

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
        registration_types = []
        registration_include_paths = []
        for registration_input in args.registration_input:
            # A registration-input file may also declare helper types (e.g. a plain data struct with
            # no attributes); only the types whose generated glue defines a registrar are wired into
            # the plugin/app entry point.
            parsed = parse_source(registration_input.read_text(encoding="utf-8"), str(registration_input))
            registrar_types = [t for t in parsed if registrar_call_lines(t, "r", "p")]
            if not registrar_types:
                continue
            registration_types.extend(registrar_types)
            registration_include_paths.append(
                resolve_include_path(registration_input, args.registration_include_root)
            )

        run_codegen(
            args.input,
            output_header,
            output_source,
            args.include_root,
            args.plugin_abi_version,
            registration_types,
            registration_include_paths,
            args.output_plugin_meta,
            args.plugin_resource_directory,
        )
    except CodegenError as error:
        print(f"tbx_attribute_codegen: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
