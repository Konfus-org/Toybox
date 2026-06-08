#!/usr/bin/env python3
from __future__ import annotations

import argparse
import sys
from pathlib import Path

from model import CodegenError
from resource_codegen import generate_builtin_asset_headers, generate_material_instance_header


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description="Generate Toybox resource helper headers.")
    parser.add_argument("--mode", choices=["builtin-assets", "material-instances"], required=True)
    parser.add_argument("--source-root", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--namespace", default="")
    args = parser.parse_args(argv)

    try:
        if args.mode == "builtin-assets":
            generate_builtin_asset_headers(args.source_root, args.output)
        else:
            generate_material_instance_header(args.source_root, args.output, args.namespace)
    except (CodegenError, OSError, ValueError) as error:
        print(f"tbx_resource_codegen: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
