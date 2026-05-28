from __future__ import annotations

from pathlib import Path

from asset_codegen import (
    emit_asset_body,
    emit_asset_meta,
    emit_asset_type_registration,
    emit_custom_asset,
    emit_text_asset,
)
from common_codegen import emit_type_name, emit_version
from enum_codegen import emit_enum
from formatter_codegen import emit_formatter
from hash_codegen import emit_hash
from model import (
    CodegenError,
    Field,
    SerializableType,
    attr_value,
    cpp_string,
    fields_of,
    find_attr,
    has_attr,
    is_asset,
    qualified_name,
    type_version,
)
from parser import parse_source
from struct_codegen import emit_indexed, emit_json_functions, emit_serializable_registration
from variant_codegen import emit_variant

GENERATED_CODE_BANNER = (
    "// GENERATED CODE ANY MODIFICATIONS WILL BE OVERWRITTEN NEXT TIME GENERATION IS RUN!"
)

PLUGIN_CATEGORY_EXPRESSIONS = {
    "default": "::tbx::PluginCategory::DEFAULT",
    "logging": "::tbx::PluginCategory::LOGGING",
    "input": "::tbx::PluginCategory::INPUT",
    "audio": "::tbx::PluginCategory::AUDIO",
    "physics": "::tbx::PluginCategory::PHYSICS",
    "rendering": "::tbx::PluginCategory::RENDERING",
    "gameplay": "::tbx::PluginCategory::GAMEPLAY",
}


def attr_values(attrs: list, name: str) -> list[str]:
    values: list[str] = []
    for attr in attrs:
        if attr.name == name:
            values.extend(attr.args)
    return values


def emit_type(type_info: SerializableType) -> list[str]:
    version = type_version(type_info)
    prop_fields = fields_of(type_info, "prop")
    type_prop_attr = find_attr(type_info.attrs, "prop")
    if not prop_fields and type_prop_attr is not None and type_prop_attr.args:
        prop_fields = [Field(name=field, kind="prop") for field in type_prop_attr.args]
    meta_fields = fields_of(type_info, "meta")
    text_fields = fields_of(type_info, "text")
    lines: list[str] = []

    if has_attr(type_info.attrs, "serializable"):
        lines.extend(emit_type_name(type_info))
        lines.extend(emit_version(type_info))

        if type_info.declaration_kind == "enum":
            lines.extend(emit_enum(type_info))
        elif type_info.declaration_kind == "using":
            count = attr_value(type_info.attrs, "count")
            if count is not None:
                lines.extend(emit_indexed(type_info, count))
            elif prop_fields:
                lines.extend(emit_json_functions(type_info, prop_fields))
                lines.extend(emit_serializable_registration(type_info))
            elif "variant" in type_info.alias_value:
                lines.extend(emit_variant(type_info))
            else:
                raise CodegenError(f"{type_info.name} is serializable but is not a std::variant alias.")
        else:
            count = attr_value(type_info.attrs, "count")
            if count is not None:
                lines.extend(emit_indexed(type_info, count))
            elif is_asset(type_info):
                if version is None:
                    raise CodegenError(f"{type_info.name} is an asset and requires [[tbx::version(N)]].")
                if len(text_fields) > 1:
                    raise CodegenError(f"{type_info.name} can only have one [[tbx::text]] field.")

                lines.extend(emit_asset_type_registration(type_info, version))
                if text_fields:
                    lines.extend(emit_text_asset(type_info, version, text_fields[0]))
                if prop_fields:
                    lines.extend(emit_asset_body(type_info, version, prop_fields))
                if meta_fields:
                    lines.extend(emit_asset_meta(type_info, version, meta_fields))
                if not text_fields and not prop_fields and not meta_fields:
                    if type_info.has_serializer:
                        lines.extend(emit_custom_asset(type_info, version))
            else:
                if text_fields or meta_fields:
                    if version is None:
                        raise CodegenError(
                            f"{type_info.name} uses asset serialization fields and requires [[tbx::version(N)]]."
                        )
                    if len(text_fields) > 1:
                        raise CodegenError(f"{type_info.name} can only have one [[tbx::text]] field.")

                    lines.extend(emit_asset_type_registration(type_info, version))
                    if text_fields:
                        lines.extend(emit_text_asset(type_info, version, text_fields[0]))
                    if meta_fields:
                        lines.extend(emit_asset_meta(type_info, version, meta_fields))
                    return lines
                if meta_fields:
                    raise CodegenError(f"{type_info.name} has [[tbx::meta]] fields but is not an Asset.")
                if text_fields:
                    raise CodegenError(f"{type_info.name} has [[tbx::text]] fields but is not an Asset.")
                if prop_fields:
                    lines.extend(emit_json_functions(type_info, prop_fields))
                    lines.extend(emit_serializable_registration(type_info))
                elif type_info.has_serializer:
                    lines.extend(emit_serializable_registration(type_info, custom=True))
                else:
                    raise CodegenError(
                        f"{type_info.name} has no serializable fields or Serializer specialization."
                    )

    return lines


def generate_header(types: list[SerializableType]) -> str:
    grouped: dict[str, list[SerializableType]] = {}
    global_lines: list[str] = []
    for type_info in types:
        grouped.setdefault(type_info.namespace, []).append(type_info)
        global_lines.extend(emit_formatter(type_info))
        global_lines.extend(emit_hash(type_info))

    lines = [
        GENERATED_CODE_BANNER,
        "#pragma once",
        "#include \"tbx/systems/assets/serialization.h\"",
        "#include \"tbx/utils/hash.h\"",
        "#include <format>",
        "#include <functional>",
        "#include <stdexcept>",
        "#include <string>",
        "#include <string_view>",
        "#include <utility>",
        "",
    ]
    for namespace, namespace_types in grouped.items():
        namespace_lines: list[str] = []
        for type_info in namespace_types:
            emitted = emit_type(type_info)
            if emitted:
                namespace_lines.append(f"// Generated serialization glue for {type_info.name}.")
                namespace_lines.extend(emitted)

        if not namespace_lines:
            continue

        if namespace:
            lines.append(f"namespace {namespace}")
            lines.append("{")
        lines.extend(namespace_lines)
        if namespace:
            lines.append("}")
            lines.append("")

    lines.extend(global_lines)
    return "\n".join(lines).rstrip() + "\n"


def validate_plugin_abi_version(plugin_abi_version: str) -> str:
    if not plugin_abi_version.isdigit():
        raise CodegenError(f"Plugin ABI version must be a non-negative integer: {plugin_abi_version}")
    return plugin_abi_version


def emit_plugin_source(
    type_info: SerializableType,
    include_path: str,
    plugin_abi_version: str,
) -> list[str]:
    plugin_name = attr_value(type_info.attrs, "name") or type_info.name
    plugin_version = attr_value(type_info.attrs, "version")
    if plugin_version is None:
        raise CodegenError(f"{type_info.name} is a plugin and requires [[tbx::version(\"...\")]].")

    raw_category = (attr_value(type_info.attrs, "category") or "default").lower()
    category_expression = PLUGIN_CATEGORY_EXPRESSIONS.get(raw_category)
    if category_expression is None:
        valid_categories = ", ".join(sorted(PLUGIN_CATEGORY_EXPRESSIONS))
        raise CodegenError(
            f"{type_info.name} uses unsupported plugin category '{raw_category}'. "
            f"Expected one of: {valid_categories}."
        )

    priority = attr_value(type_info.attrs, "priority") or "0"
    if not priority.isdigit():
        raise CodegenError(f"{type_info.name} plugin priority must be a non-negative integer.")

    description = attr_value(type_info.attrs, "description")
    dependencies = attr_values(type_info.attrs, "dependency")
    qualified_plugin_name = qualified_name(type_info)

    dependency_entries = ", ".join(cpp_string(dependency) for dependency in dependencies)
    dependency_initializer = "{" + dependency_entries + "}"
    validated_plugin_abi_version = validate_plugin_abi_version(plugin_abi_version)

    lines = [
        GENERATED_CODE_BANNER,
        f"#include {cpp_string(include_path)}",
        "#include \"tbx/interfaces/plugin.h\"",
        "#include \"tbx/systems/plugin_api/plugin_meta.h\"",
        "",
        "TBX_PLUGIN_ENTRY_EXPORT void tbx_get_plugin_meta(::tbx::PluginMeta* out_meta)",
        "{",
        "    if (out_meta == nullptr)",
        "        return;",
        "",
        "    auto meta = ::tbx::PluginMeta();",
        f"    meta.name = {cpp_string(plugin_name)};",
        f"    meta.version = {cpp_string(plugin_version)};",
        f"    meta.abi_version = {validated_plugin_abi_version}U;",
        f"    meta.category = {category_expression};",
        f"    meta.priority = {priority}U;",
        "    meta.linkage = ::tbx::PluginLinkage::DYNAMIC;",
        f"    meta.dependencies = {dependency_initializer};",
    ]
    if description is not None:
        lines.append(f"    meta.description = {cpp_string(description)};")

    lines.extend(
        [
            "    *out_meta = meta;",
            "}",
            "",
            "TBX_PLUGIN_ENTRY_EXPORT ::tbx::Plugin* tbx_create_plugin()",
            "{",
            f"    ::tbx::Plugin* plugin = new {qualified_plugin_name}();",
            f"    ::tbx::PluginRegistry::get_instance().register_plugin({cpp_string(plugin_name)}, plugin);",
            "    return plugin;",
            "}",
            "",
            "TBX_PLUGIN_ENTRY_EXPORT void tbx_destroy_plugin(::tbx::Plugin* plugin)",
            "{",
            f"    ::tbx::PluginRegistry::get_instance().unregister_plugin({cpp_string(plugin_name)});",
            "    delete plugin;",
            "}",
            "",
        ]
    )
    return lines


def generate_source(
    header_name: str,
    types: list[SerializableType] | None = None,
    include_path: str | None = None,
    plugin_abi_version: str = "1",
) -> str:
    plugin_types = [type_info for type_info in types or [] if has_attr(type_info.attrs, "plugin")]
    if len(plugin_types) > 1:
        names = ", ".join(type_info.name for type_info in plugin_types)
        raise CodegenError(f"Only one [[tbx::plugin]] declaration is supported per generated source: {names}.")

    if plugin_types:
        if include_path is None:
            raise CodegenError(f"{plugin_types[0].name} plugin generation requires an include path.")
        return (
            "\n".join(emit_plugin_source(plugin_types[0], include_path, plugin_abi_version)).rstrip()
            + "\n"
        )

    lines = [
        GENERATED_CODE_BANNER,
        f"// Generated declarations are header-only for attribute reflection glue: {header_name}",
        "",
    ]
    return "\n".join(lines).rstrip() + "\n"


def write_if_different(output_path: Path, output: str) -> None:
    output_path.parent.mkdir(parents=True, exist_ok=True)
    if output_path.exists() and output_path.read_text(encoding="utf-8") == output:
        return
    output_path.write_text(output, encoding="utf-8", newline="\n")


def resolve_include_path(input_path: Path, include_root: Path | None) -> str:
    if include_root is not None:
        try:
            return input_path.resolve().relative_to(include_root.resolve()).as_posix()
        except ValueError:
            pass

    return input_path.name


def run_codegen(
    input_path: Path,
    output_header_path: Path,
    output_source_path: Path,
    include_root: Path | None = None,
    plugin_abi_version: str = "1",
) -> None:
    source = input_path.read_text(encoding="utf-8")
    types = parse_source(source, str(input_path))
    write_if_different(output_header_path, generate_header(types))
    include_path = resolve_include_path(input_path, include_root)
    write_if_different(
        output_source_path,
        generate_source(output_header_path.name, types, include_path, plugin_abi_version),
    )
