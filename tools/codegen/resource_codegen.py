from __future__ import annotations

import json
import re
from pathlib import Path

from model import CodegenError


def make_snake_identifier(raw_name: str) -> str:
    normalized = re.sub(r"([a-z0-9])([A-Z])", r"\1_\2", raw_name).lower()
    normalized = re.sub(r"[^a-z0-9_]", "_", normalized)
    normalized = re.sub(r"_+", "_", normalized).strip("_")
    if not normalized:
        raise CodegenError(f"resource_codegen: invalid identifier '{raw_name}'")
    if normalized[0].isdigit():
        normalized = "_" + normalized
    return normalized


def make_pascal_identifier(raw_name: str) -> str:
    normalized = re.sub(r"[^A-Za-z0-9_]", "_", raw_name)
    parts = [part for part in re.split(r"_+", normalized) if part]
    identifier = "".join(part[:1].upper() + part[1:] for part in parts)
    if not identifier:
        raise CodegenError(f"resource_codegen: invalid type name '{raw_name}'")
    if identifier[0].isdigit():
        identifier = "_" + identifier
    return identifier


def make_singular_identifier(raw_name: str) -> str:
    if raw_name.endswith("ies"):
        return raw_name[:-3] + "y"
    if raw_name.endswith("s"):
        return raw_name[:-1]
    return raw_name


def write_if_different(output_path: Path, output: str) -> None:
    output_path.parent.mkdir(parents=True, exist_ok=True)
    if output_path.exists() and output_path.read_text(encoding="utf-8") == output:
        return
    output_path.write_text(output, encoding="utf-8", newline="\n")


def read_json_file(input_path: Path) -> dict:
    return json.loads(input_path.read_text(encoding="utf-8-sig"))


def read_binding_list(material_data: dict, key: str) -> list:
    value = material_data.get(key, [])
    if isinstance(value, list):
        return value
    if isinstance(value, dict):
        values = value.get("values", [])
        if isinstance(values, list):
            return values
    return []


def read_meta_id(meta_path: Path) -> int:
    if not meta_path.exists():
        raise CodegenError(f"resource_codegen: missing meta file '{meta_path}'")

    data = read_json_file(meta_path)
    value = data.get("id")
    if not isinstance(value, int) or isinstance(value, bool) or value <= 0:
        raise CodegenError(f"resource_codegen: missing id in '{meta_path}'")
    return value


def is_builtin_asset_meta(relative_meta_path: Path) -> bool:
    relative_text = relative_meta_path.as_posix()
    if relative_text.startswith("generated/"):
        return False
    if relative_meta_path.name == "CMakeLists.txt.meta":
        return False
    if re.search(r"\.(cmake|h|hh|hpp|c|cc|cpp|cxx|in)\.meta$", relative_text):
        return False
    if relative_text.endswith(".mat.meta"):
        return False
    return True


def make_builtin_struct_identifier(relative_meta_path: Path) -> str:
    relative_asset_path = Path(relative_meta_path.as_posix().removesuffix(".meta"))
    asset_directory = relative_asset_path.parent.as_posix()
    asset_stem = relative_asset_path.stem
    asset_extension = relative_asset_path.suffix.lower()

    top_level_group = ""
    remaining_directory = ""
    if asset_directory and asset_directory != ".":
        parts = asset_directory.split("/")
        top_level_group = parts[0]
        remaining_directory = "/".join(parts[1:])

    identifier_source = ""
    if remaining_directory:
        identifier_source += remaining_directory.replace("/", "_") + "_"

    if asset_extension == ".vert":
        identifier_source += f"{asset_stem}_vertex_shader"
    elif asset_extension == ".frag":
        identifier_source += f"{asset_stem}_fragment_shader"
    elif asset_extension == ".geom":
        identifier_source += f"{asset_stem}_geometry_shader"
    elif asset_extension == ".glsl":
        identifier_source += f"{asset_stem}_shader_library"
    else:
        identifier_source += asset_stem
        if top_level_group:
            identifier_source += f"_{make_singular_identifier(top_level_group)}"

    return make_pascal_identifier(identifier_source)


def make_builtin_group_identifier(relative_meta_path: Path) -> str:
    relative_asset_path = Path(relative_meta_path.as_posix().removesuffix(".meta"))
    asset_directory = relative_asset_path.parent.as_posix()
    if asset_directory and asset_directory != ".":
        return make_snake_identifier(asset_directory.split("/")[0])
    return "root"


def generate_builtin_asset_headers(source_root: Path, output_file: Path) -> None:
    source_root = source_root.resolve()
    output_file = output_file.resolve()
    output_directory = output_file.parent

    meta_files = sorted(
        path
        for path in source_root.rglob("*.meta")
        if is_builtin_asset_meta(path.relative_to(source_root))
    )
    if not meta_files:
        raise CodegenError("resource_codegen: no resource meta files found")

    entries: list[tuple[str, str, int]] = []
    used_struct_names: set[str] = set()
    for meta_file in meta_files:
        relative_meta_path = meta_file.relative_to(source_root)
        struct_name = make_builtin_struct_identifier(relative_meta_path)
        if struct_name in used_struct_names:
            raise CodegenError(f"resource_codegen: duplicate builtin asset struct '{struct_name}'")
        used_struct_names.add(struct_name)

        group_identifier = make_builtin_group_identifier(relative_meta_path)
        entries.append((group_identifier, struct_name, read_meta_id(meta_file)))

    groups = sorted({group for group, _, _ in entries})
    expected_group_headers: list[Path] = []
    for group in groups:
        group_lines = [
            "#pragma once",
            "#include \"tbx/types/handle.h\"",
            "",
            "namespace tbx",
            "{",
        ]
        for entry_group, struct_name, id_value in sorted(entries):
            if entry_group != group:
                continue
            group_lines.extend(
                [
                    "",
                    "    /// @brief",
                    "    /// Purpose: Typed built-in asset handle generated from bundled resources.",
                    "    /// @details",
                    "    /// Ownership: Stores the asset handle by value.",
                    "    /// Thread Safety: Safe to read concurrently.",
                    f"    struct {struct_name} final",
                    "    {",
                    f"        static inline const Handle HANDLE = Handle(Uuid({id_value}U));",
                    "    };",
                ]
            )
        group_lines.append("}")
        group_header = output_directory / f"builtin_assets_{group}.generated.h"
        write_if_different(group_header, "\n".join(group_lines).rstrip() + "\n")
        expected_group_headers.append(group_header)

    for existing_group_header in output_directory.glob("builtin_assets_*.generated.h"):
        if existing_group_header.name == output_file.name:
            continue
        if existing_group_header.resolve() not in {path.resolve() for path in expected_group_headers}:
            existing_group_header.unlink()

    root_lines = ["#pragma once"]
    for group_header in expected_group_headers:
        root_lines.append(f"#include \"{group_header.name}\"")
    write_if_different(output_file, "\n".join(root_lines).rstrip() + "\n")


def make_material_binding_identifier(raw_name: str) -> str:
    identifier_source = raw_name.replace('"', "")
    if len(identifier_source) > 1 and identifier_source[1] == "_":
        identifier_source = identifier_source[2:]
    return re.sub(r"[^A-Za-z0-9_]", "_", identifier_source).upper()


def generate_material_instance_header(source_root: Path, output_file: Path, namespace: str) -> None:
    source_root = source_root.resolve()
    material_files = sorted(
        path for path in source_root.rglob("*.mat") if "generated" not in path.parts
    )

    lines = [
        "#pragma once",
        "#include \"tbx/types/handle.h\"",
        "#include <string>",
        "",
    ]
    if namespace:
        lines.extend([f"namespace {namespace}", "{"])

    used_struct_names: set[str] = set()
    for material_file in material_files:
        relative_material_path = material_file.relative_to(source_root).as_posix()
        meta_file = material_file.with_name(material_file.name + ".meta")
        material_id_value = read_meta_id(meta_file)
        struct_name = make_pascal_identifier(material_file.stem) + "Material"
        if struct_name in used_struct_names:
            raise CodegenError(f"resource_codegen: duplicate material struct '{struct_name}'")
        used_struct_names.add(struct_name)

        material_data = read_json_file(material_file)
        binding_lines: list[str] = []
        used_binding_names: set[str] = set()
        binding_sources = []
        binding_sources.extend(read_binding_list(material_data, "textures"))
        binding_sources.extend(read_binding_list(material_data, "parameters"))
        for binding in binding_sources:
            parameter_type = str(binding.get("type", binding.get("data", {}).get("type", ""))).lower()
            if parameter_type == "shader":
                continue

            binding_name = binding.get("name")
            if not isinstance(binding_name, str) or not binding_name:
                continue

            identifier = make_material_binding_identifier(binding_name)
            if identifier in used_binding_names:
                raise CodegenError(f"resource_codegen: duplicate generated material key '{identifier}'")
            used_binding_names.add(identifier)
            binding_lines.append(f"        static inline const std::string {identifier} = \"{binding_name}\";")

        lines.extend(
            [
                "",
                "    /// @brief",
                f"    /// Purpose: Typed material keys generated from '{relative_material_path}'.",
                "    /// @details",
                "    /// Ownership: Stores the material handle by value and exposes parameter and texture names as strings.",
                "    /// Thread Safety: Safe to read concurrently.",
                f"    struct {struct_name} final",
                "    {",
                f"        static inline const tbx::Handle HANDLE = tbx::Handle(tbx::Uuid({material_id_value}U));",
                *binding_lines,
                "    };",
            ]
        )

    if namespace:
        lines.append("}")

    write_if_different(output_file, "\n".join(lines).rstrip() + "\n")
