from __future__ import annotations

import re
from pathlib import Path

from asset_codegen import (
    emit_asset_body,
    emit_asset_body_declarations,
    emit_asset_meta,
    emit_asset_meta_declarations,
    emit_asset_type_registration,
    emit_asset_type_registration_declarations,
    emit_custom_asset,
    emit_custom_asset_declarations,
    emit_text_asset,
    emit_text_asset_declarations,
)
from common_codegen import emit_type_name, emit_version
from enum_codegen import emit_enum, emit_enum_declarations
from formatter_codegen import emit_formatter, emit_formatter_declaration
from hash_codegen import (
    emit_hash,
    emit_hash_declaration,
    emit_hash_equality,
    emit_hash_equality_declaration,
)
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
    json_key,
    qualified_name,
    type_version,
)
from parser import parse_source
from struct_codegen import (
    emit_custom_serializable_registration,
    emit_indexed,
    emit_json_function_declarations,
    emit_json_function_definitions,
    emit_lifecycle_hook_declarations,
    emit_lifecycle_hook_definitions,
    emit_serializable_registration,
    emit_struct_serialization_declarations,
)
from variant_codegen import emit_variant, emit_variant_declarations

GENERATED_CODE_BANNER = (
    "// GENERATED CODE ANY MODIFICATIONS WILL BE OVERWRITTEN NEXT TIME GENERATION IS RUN!"
)
INCLUDE_PATTERN = re.compile(r'^\s*#include\s+"([^"]+)"', re.MULTILINE)

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


def resolve_custom_serialization_callable(type_info: SerializableType, callable_name: str) -> str:
    normalized = callable_name.strip()
    if "::" in normalized or "(" in normalized:
        return normalized
    if type_info.declaration_kind == "using" and type_info.namespace:
        return f"::{type_info.namespace}::{normalized}"
    return f"{type_info.name}::{normalized}"


def serializable_mode(type_info: SerializableType) -> str:
    attr = find_attr(type_info.attrs, "serializable")
    if attr is None or not attr.args:
        return "json"

    mode = attr.args[0]
    if mode not in {"json", "text"}:
        raise CodegenError(
            f"{type_info.name} uses unsupported serializable mode '{mode}'. "
            "Expected 'json' or 'text'."
        )
    return mode


def emit_script_asset_declarations(type_info: SerializableType, prop_fields: list[Field]) -> list[str]:
    override_helper = f"tbx_apply_script_overrides_{type_info.name}"
    bind_helper = f"tbx_bind_script_runtime_{type_info.name}"
    return [
        f"std::true_type tbx_has_asset_serialization(const {type_info.name}*);",
        *emit_json_function_declarations(type_info),
        f"::tbx::Result {override_helper}(const ::tbx::Json& tbx_json, {type_info.name}& tbx_value);",
        f"void {bind_helper}({type_info.name}& tbx_value, ::tbx::ScriptContext& tbx_context);",
        "",
    ]


def emit_script_asset(type_info: SerializableType, version: str, prop_fields: list[Field]) -> list[str]:
    if "Script" not in type_info.bases:
        raise CodegenError(f"{type_info.name} uses [[tbx::script]] but does not derive from tbx::Script.")

    bind_fields = prop_fields + fields_of(type_info, "inject")
    override_helper = f"tbx_apply_script_overrides_{type_info.name}"
    bind_helper = f"tbx_bind_script_runtime_{type_info.name}"
    lines = [
        f"std::true_type tbx_has_asset_serialization(const {type_info.name}*)",
        "{",
        "    return {};",
        "}",
    ]
    lines.extend(emit_json_function_definitions(type_info, prop_fields))
    lines.extend(
        [
            f"::tbx::Result {override_helper}(const ::tbx::Json& tbx_json, {type_info.name}& tbx_value)",
            "{",
            "    try",
            "    {",
            "        if (!tbx_json.is_object())",
            "            return {};",
        ]
    )
    for field in prop_fields:
        lines.extend(
            [
                f"        if (const auto tbx_value_it = tbx_json.find({cpp_string(json_key(field))}); tbx_value_it != tbx_json.end())",
                f"            ::tbx::read_serialization_value(*tbx_value_it, tbx_value.{field.name});",
            ]
        )
    lines.extend(
        [
            "        return {};",
            "    }",
            "    catch (const std::exception& exception)",
            "    {",
            "        return ::tbx::make_serialization_failure(",
            f"            std::string(\"Failed to apply script overrides for {type_info.name}: \").append(exception.what()));",
            "    }",
            "    catch (...)",
            "    {",
            "        return ::tbx::make_serialization_failure(",
            f"            \"Failed to apply script overrides for {type_info.name}.\");",
            "    }",
            "}",
            f"void {bind_helper}({type_info.name}& tbx_value, ::tbx::ScriptContext& tbx_context)",
            "{",
        ]
    )
    for field in bind_fields:
        lines.append(f"    ::tbx::bind_script_field(tbx_value.{field.name}, tbx_context);")
    lines.extend(
        [
            "}",
            "TBX_SERIALIZATION_AUTO_REGISTER(",
            "    tbx_script_asset_type_registration_,",
            f"    ::tbx::register_script_asset_type<{type_info.name}>(",
            f"        {version},",
            f"        {override_helper},",
            f"        {bind_helper}));",
            "",
        ]
    )
    return lines


def emit_type(type_info: SerializableType, target: str) -> list[str]:
    version = type_version(type_info)
    prop_fields = fields_of(type_info, "prop")
    type_prop_attr = find_attr(type_info.attrs, "prop")
    type_meta_attr = find_attr(type_info.attrs, "meta")
    if type_prop_attr is not None:
        raise CodegenError(
            f"{type_info.name} uses unsupported type-level [[tbx::prop(...)]] fields. "
            "Place [[tbx::prop]] on each exposed property instead."
        )
    if type_meta_attr is not None:
        raise CodegenError(
            f"{type_info.name} uses unsupported type-level [[tbx::meta(...)]] fields. "
            "Place [[tbx::meta]] on each exposed property instead."
        )
    meta_fields = fields_of(type_info, "meta")
    text_fields = fields_of(type_info, "text")
    lines: list[str] = []
    custom_serialization_attr = find_attr(type_info.attrs, "custom_serialization")
    custom_write_callable: str | None = None
    custom_read_callable: str | None = None
    if custom_serialization_attr is not None:
        if len(custom_serialization_attr.args) != 2:
            raise CodegenError(
                f"{type_info.name} uses [[tbx::custom_serialization]] with an invalid argument list. "
                "Expected [[tbx::custom_serialization(write_fn, read_fn)]]."
            )
        custom_write_callable = resolve_custom_serialization_callable(
            type_info,
            custom_serialization_attr.args[0],
        )
        custom_read_callable = resolve_custom_serialization_callable(
            type_info,
            custom_serialization_attr.args[1],
        )

    if has_attr(type_info.attrs, "script"):
        if version is None:
            raise CodegenError(f"{type_info.name} is a script and requires [[tbx::version(N)]].")
        if target == "header":
            lines.extend(emit_type_name(type_info))
            lines.extend(emit_version(type_info))
            lines.extend(emit_lifecycle_hook_declarations(type_info))
            lines.extend(emit_script_asset_declarations(type_info, prop_fields))
            lines.extend(emit_hash_equality_declaration(type_info))
        else:
            lines.extend(emit_lifecycle_hook_definitions(type_info))
            lines.extend(emit_script_asset(type_info, version, prop_fields))
            lines.extend(emit_hash_equality(type_info))
        return lines

    if has_attr(type_info.attrs, "serializable"):
        mode = serializable_mode(type_info)
        if target == "header":
            lines.extend(emit_type_name(type_info))
            lines.extend(emit_version(type_info))
            lines.extend(emit_lifecycle_hook_declarations(type_info))
        else:
            lines.extend(emit_lifecycle_hook_definitions(type_info))

        if type_info.declaration_kind == "enum":
            if mode != "json":
                raise CodegenError(f"{type_info.name} enum serialization only supports json mode.")
            lines.extend(emit_enum_declarations(type_info) if target == "header" else emit_enum(type_info))
        elif type_info.declaration_kind == "using":
            if mode != "json":
                raise CodegenError(f"{type_info.name} alias serialization only supports json mode.")
            array_count = attr_value(type_info.attrs, "array")
            if array_count is not None:
                if target == "header":
                    lines.extend(emit_struct_serialization_declarations(type_info))
                else:
                    lines.extend(emit_indexed(type_info, array_count))
            elif custom_write_callable is not None and custom_read_callable is not None:
                if target == "header":
                    lines.extend(emit_struct_serialization_declarations(type_info))
                else:
                    lines.extend(
                        emit_custom_serializable_registration(
                            type_info,
                            custom_write_callable,
                            custom_read_callable,
                        )
                    )
            elif prop_fields:
                if target == "header":
                    lines.extend(emit_struct_serialization_declarations(type_info))
                else:
                    lines.extend(emit_json_function_definitions(type_info, prop_fields))
                    lines.extend(emit_serializable_registration(type_info))
            elif "variant" in type_info.alias_value:
                lines.extend(
                    emit_variant_declarations(type_info) if target == "header" else emit_variant(type_info)
                )
            else:
                raise CodegenError(f"{type_info.name} is serializable but is not a std::variant alias.")
        else:
            array_count = attr_value(type_info.attrs, "array")
            if array_count is not None:
                if mode != "json":
                    raise CodegenError(f"{type_info.name} indexed serialization only supports json mode.")
                if target == "header":
                    lines.extend(emit_struct_serialization_declarations(type_info))
                else:
                    lines.extend(emit_indexed(type_info, array_count))
            elif is_asset(type_info):
                if version is None:
                    raise CodegenError(f"{type_info.name} is an asset and requires [[tbx::version(N)]].")
                if len(text_fields) > 1:
                    raise CodegenError(f"{type_info.name} can only have one [[tbx::text]] field.")
                if mode == "text" and text_fields:
                    raise CodegenError(
                        f"{type_info.name} text mode uses one [[tbx::prop]] field instead of [[tbx::text]]."
                    )
                if mode == "text" and len(prop_fields) != 1:
                    raise CodegenError(
                        f"{type_info.name} text mode requires exactly one [[tbx::prop]] field."
                    )

                lines.extend(
                    emit_asset_type_registration_declarations(type_info)
                    if target == "header"
                    else emit_asset_type_registration(type_info, version)
                )
                if mode == "text":
                    lines.extend(
                        emit_text_asset_declarations(type_info)
                        if target == "header"
                        else emit_text_asset(type_info, version, prop_fields[0])
                    )
                elif text_fields:
                    lines.extend(
                        emit_text_asset_declarations(type_info)
                        if target == "header"
                        else emit_text_asset(type_info, version, text_fields[0])
                    )
                if mode == "json" and prop_fields:
                    lines.extend(
                        emit_asset_body_declarations(type_info)
                        if target == "header"
                        else emit_asset_body(type_info, version, prop_fields)
                    )
                if meta_fields:
                    lines.extend(
                        emit_asset_meta_declarations(type_info)
                        if target == "header"
                        else emit_asset_meta(type_info, version, meta_fields)
                    )
                if mode == "json" and not text_fields and not prop_fields and not meta_fields:
                    if custom_write_callable is not None and custom_read_callable is not None:
                        if target == "header":
                            lines.extend(emit_custom_asset_declarations(type_info))
                        else:
                            lines.extend(
                                emit_custom_asset(
                                    type_info,
                                    version,
                                    custom_write_callable,
                                    custom_read_callable,
                                )
                            )
                    elif type_info.has_serializer:
                        if target == "header":
                            lines.extend(emit_custom_asset_declarations(type_info))
                        else:
                            lines.extend(
                                emit_custom_asset(
                                    type_info,
                                    version,
                                    f"::tbx::Serializer<{type_info.name}>::serialize",
                                    f"::tbx::Serializer<{type_info.name}>::deserialize",
                                )
                            )
            else:
                if mode != "json":
                    raise CodegenError(f"{type_info.name} text mode is only supported for assets.")
                if text_fields or meta_fields:
                    if version is None:
                        raise CodegenError(
                            f"{type_info.name} uses asset serialization fields and requires [[tbx::version(N)]]."
                        )
                    if len(text_fields) > 1:
                        raise CodegenError(f"{type_info.name} can only have one [[tbx::text]] field.")

                    lines.extend(
                        emit_asset_type_registration_declarations(type_info)
                        if target == "header"
                        else emit_asset_type_registration(type_info, version)
                    )
                    if text_fields:
                        lines.extend(
                            emit_text_asset_declarations(type_info)
                            if target == "header"
                            else emit_text_asset(type_info, version, text_fields[0])
                        )
                    if meta_fields:
                        lines.extend(
                            emit_asset_meta_declarations(type_info)
                            if target == "header"
                            else emit_asset_meta(type_info, version, meta_fields)
                        )
                    if target == "header":
                        lines.extend(emit_hash_equality_declaration(type_info))
                    else:
                        lines.extend(emit_hash_equality(type_info))
                    return lines
                if meta_fields:
                    raise CodegenError(f"{type_info.name} has [[tbx::meta]] fields but is not an Asset.")
                if text_fields:
                    raise CodegenError(f"{type_info.name} has [[tbx::text]] fields but is not an Asset.")
                if prop_fields:
                    if target == "header":
                        lines.extend(emit_struct_serialization_declarations(type_info))
                    else:
                        lines.extend(emit_json_function_definitions(type_info, prop_fields))
                        lines.extend(emit_serializable_registration(type_info))
                elif custom_write_callable is not None and custom_read_callable is not None:
                    if target == "header":
                        lines.extend(emit_struct_serialization_declarations(type_info))
                    else:
                        lines.extend(
                            emit_custom_serializable_registration(
                                type_info,
                                custom_write_callable,
                                custom_read_callable,
                            )
                        )
                elif type_info.has_serializer:
                    if target == "header":
                        lines.extend(emit_struct_serialization_declarations(type_info))
                    else:
                        lines.extend(emit_serializable_registration(type_info, custom=True))
                else:
                    raise CodegenError(
                        f"{type_info.name} has no serializable fields or Serializer specialization."
                    )

    if target == "header":
        lines.extend(emit_hash_equality_declaration(type_info))
    else:
        lines.extend(emit_hash_equality(type_info))

    return lines


def emit_forward_declaration(type_info: SerializableType) -> list[str]:
    if type_info.declaration_kind == "enum" and not type_info.enum_scoped:
        return []

    if type_info.declaration_kind == "using":
        declaration = f"using {type_info.name} = {type_info.alias_value};"
    elif type_info.declaration_kind == "enum":
        underlying_type = (
            f" : {type_info.enum_underlying_type}" if type_info.enum_underlying_type else ""
        )
        declaration = f"enum class {type_info.name}{underlying_type};"
    else:
        api_prefix = f" {type_info.api_macro}" if type_info.api_macro else ""
        declaration = f"{type_info.declaration_kind}{api_prefix} {type_info.name};"

    if not type_info.namespace:
        return [declaration]

    return [
        f"namespace {type_info.namespace}",
        "{",
        f"    {declaration}",
        "}",
    ]


def emit_forward_declarations(types: list[SerializableType]) -> list[str]:
    lines: list[str] = []
    emitted: set[tuple[str, str]] = set()
    for type_info in types:
        key = (type_info.namespace, type_info.name)
        if key in emitted:
            continue

        declaration_lines = emit_forward_declaration(type_info)
        if not declaration_lines:
            continue

        if lines:
            lines.append("")
        lines.extend(declaration_lines)
        emitted.add(key)

    if lines:
        lines.append("")
    return lines


def generate_header(types: list[SerializableType]) -> str:
    grouped: dict[str, list[SerializableType]] = {}
    global_lines: list[str] = []
    for type_info in types:
        grouped.setdefault(type_info.namespace, []).append(type_info)
        global_lines.extend(emit_formatter_declaration(type_info))
        global_lines.extend(emit_hash_declaration(type_info))

    lines = [
        GENERATED_CODE_BANNER,
        "#pragma once",
        "#include \"tbx/systems/assets/serialization.h\"",
        "#include \"tbx/types/typedefs.h\"",
        "#include <cstdint>",
        "#include <format>",
        "#include <functional>",
        "#include <stdexcept>",
        "#include <string>",
        "#include <string_view>",
        "#include <utility>",
        "#include <variant>",
        "#include <vector>",
        "",
    ]
    lines.extend(emit_forward_declarations(types))
    for namespace, namespace_types in grouped.items():
        namespace_lines: list[str] = []
        for type_info in namespace_types:
            emitted = emit_type(type_info, "header")
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


def generate_type_source(
    header_name: str,
    types: list[SerializableType],
    include_path: str,
) -> str:
    if not types:
        lines = [
            GENERATED_CODE_BANNER,
            f"// No attribute reflection glue was discovered for {header_name}.",
            "",
        ]
        return "\n".join(lines).rstrip() + "\n"

    grouped: dict[str, list[SerializableType]] = {}
    global_lines: list[str] = []
    for type_info in types:
        grouped.setdefault(type_info.namespace, []).append(type_info)
        global_lines.extend(emit_formatter(type_info))
        global_lines.extend(emit_hash(type_info))

    lines = [
        GENERATED_CODE_BANNER,
        f"#include {cpp_string(include_path)}",
        f"#include {cpp_string(header_name)}",
    ]
    if global_lines:
        lines.append("#include \"tbx/utils/hash.h\"")
    lines.append("")
    for namespace, namespace_types in grouped.items():
        namespace_lines: list[str] = []
        for type_info in namespace_types:
            emitted = emit_type(type_info, "source")
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
    script_types: list[SerializableType] | None = None,
    script_include_paths: list[str] | None = None,
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
        "#include \"tbx/systems/assets/serialization.h\"",
        "#include \"tbx/systems/plugin_api/plugin_meta.h\"",
    ]
    if script_types:
        for script_include_path in sorted(set(script_include_paths or [])):
            lines.append(f"#include {cpp_string(script_include_path)}")
    lines.extend(
        [
            "",
            "TBX_PLUGIN_ENTRY_EXPORT void tbx_get_plugin_meta(::tbx::PluginMeta* out_meta)",
        ]
    )
    lines.extend(
        [
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
            "#if defined(TBX_PLUGIN_RESOURCE_DIRECTORY)",
            "    meta.resource_directory = TBX_PLUGIN_RESOURCE_DIRECTORY;",
            "#endif",
        ]
    )
    if description is not None:
        lines.append(f"    meta.description = {cpp_string(description)};")

    lines.extend(
        [
            "    *out_meta = meta;",
            "}",
            "",
        ]
    )

    if script_types:
        lines.extend(
            [
                "TBX_PLUGIN_ENTRY_EXPORT void tbx_register_plugin_scripts()",
                "{",
            ]
        )
        for script_type in script_types:
            version = type_version(script_type)
            if version is None:
                raise CodegenError(
                    f"{script_type.name} is a script and requires [[tbx::version(N)]]."
                )
            namespace_prefix = f"{script_type.namespace}::" if script_type.namespace else ""
            lines.extend(
                [
                    f"    static_cast<void>(::tbx::register_script_asset_type<{qualified_name(script_type)}>(",
                    f"        {version},",
                    f"        {namespace_prefix}tbx_apply_script_overrides_{script_type.name},",
                    f"        {namespace_prefix}tbx_bind_script_runtime_{script_type.name}));",
                ]
            )
        lines.extend(["}", ""])
        lines.extend(
            [
                "TBX_PLUGIN_ENTRY_EXPORT void tbx_unregister_plugin_scripts()",
                "{",
            ]
        )
        for script_type in script_types:
            lines.append(
                f"    ::tbx::unregister_asset_type_entry(std::type_index(typeid({qualified_name(script_type)})));"
            )
        lines.extend(["}", ""])

    lines.extend(
        [
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
    script_types: list[SerializableType] | None = None,
    script_include_paths: list[str] | None = None,
) -> str:
    plugin_types = [type_info for type_info in types or [] if has_attr(type_info.attrs, "plugin")]
    if len(plugin_types) > 1:
        names = ", ".join(type_info.name for type_info in plugin_types)
        raise CodegenError(f"Only one [[tbx::plugin]] declaration is supported per generated source: {names}.")

    if plugin_types:
        if include_path is None:
            raise CodegenError(f"{plugin_types[0].name} plugin generation requires an include path.")
        return (
            "\n".join(
                emit_plugin_source(
                    plugin_types[0],
                    include_path,
                    plugin_abi_version,
                    script_types,
                    script_include_paths,
                )
            ).rstrip()
            + "\n"
        )

    if include_path is None:
        raise CodegenError("Attribute source generation requires an include path.")
    return generate_type_source(header_name, types or [], include_path)

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


def read_include_context(input_path: Path, include_root: Path | None) -> str:
    source = input_path.read_text(encoding="utf-8")
    context: list[str] = []
    for match in INCLUDE_PATTERN.finditer(source):
        include_name = match.group(1)
        if ".generated." in include_name:
            continue

        candidates = [input_path.parent / include_name]
        if include_root is not None:
            candidates.append(include_root / include_name)

        for candidate in candidates:
            if not candidate.exists() or candidate.resolve() == input_path.resolve():
                continue
            context.append(candidate.read_text(encoding="utf-8"))
            break

    return "\n".join(context)


def run_codegen(
    input_path: Path,
    output_header_path: Path,
    output_source_path: Path,
    include_root: Path | None = None,
    plugin_abi_version: str = "1",
    script_types: list[SerializableType] | None = None,
    script_include_paths: list[str] | None = None,
) -> None:
    source = input_path.read_text(encoding="utf-8")
    types = parse_source(source, str(input_path), read_include_context(input_path, include_root))
    write_if_different(output_header_path, generate_header(types))
    include_path = resolve_include_path(input_path, include_root)
    write_if_different(
        output_source_path,
        generate_source(
            output_header_path.name,
            types,
            include_path,
            plugin_abi_version,
            script_types,
            script_include_paths,
        ),
    )
