"""Toybox attribute code generator.

The parser produces neutral metadata. This module owns code emission by running
that metadata through independent processors for serialization, services,
formatting, hashing, and plugins.
"""

from __future__ import annotations

import json
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
from model import (
    Attribute,
    CodegenError,
    Field,
    SerializableType,
    attr_arg,
    attr_list_arg,
    attr_value,
    attrs_named,
    cpp_string,
    fields_of,
    find_attr,
    has_attr,
    is_asset,
    json_key,
    qualified_name,
    serialized_fields,
    type_version,
)
from parser import parse_source
from processors import (
    AppProcessor,
    CodegenContext,
    CodegenRegistry,
    FormatterProcessor,
    HashProcessor,
    PluginProcessor,
    SerializationProcessor,
    ServiceBindingProcessor,
)
from struct_codegen import (
    emit_custom_serializable_registration,
    emit_indexed,
    emit_json_function_declarations,
    emit_json_function_definitions,
    emit_lifecycle_hook_declarations,
    emit_lifecycle_hook_definitions,
    emit_serializable_registration,
    emit_struct_serialization_declarations,
    emit_struct_value_serialization,
    emit_template_value_serialization,
    emit_typed_write_field,
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
    "scripting": "::tbx::PluginCategory::SCRIPTING",
    "gameplay": "::tbx::PluginCategory::GAMEPLAY",
}

GAMEPLAY_PLUGIN_DEFAULT_DEPENDENCIES = [
    "SdlBaseSystemsPlugin",
    "SdlWindowingPlugin",
    "SdlOpenGlContextManagerPlugin",
    "OpenGlRenderingPlugin",
    "SdlInputPlugin",
    "JoltPhysicsPlugin",
    "AssimpModelLoaderPlugin",
    "StbImageLoaderPlugin",
    "ShaderIncludeLoader",
    "PerformanceMonitor",
]


def service_register_fields(type_info: SerializableType) -> list[Field]:
    return fields_of(type_info, "register")


def service_register_attrs(type_info: SerializableType) -> list[Attribute]:
    return attrs_named(type_info.attrs, "register")


def plugin_metadata_arg(type_info: SerializableType, index: int, default: str | None = None) -> str | None:
    plugin_attr = find_attr(type_info.attrs, "register_plugin")
    if plugin_attr is None:
        return default
    names = ["name", "version", "category", "priority"]
    return attr_arg(plugin_attr, index, names[index], default)


def plugin_category_expression(type_info: SerializableType) -> tuple[str, str]:
    raw_category = plugin_metadata_arg(type_info, 2, "default") or "default"
    if "PluginCategory::" in raw_category:
        category_key = raw_category.rsplit("::", 1)[-1].lower()
        return raw_category, category_key

    category_key = raw_category.lower()
    category_expression = PLUGIN_CATEGORY_EXPRESSIONS.get(category_key)
    if category_expression is None:
        valid_categories = ", ".join(sorted(PLUGIN_CATEGORY_EXPRESSIONS))
        raise CodegenError(
            f"{type_info.name} uses unsupported plugin category '{raw_category}'. "
            f"Expected one of: {valid_categories}, or a PluginCategory enum expression."
        )

    return category_expression, category_key


def plugin_dependencies(type_info: SerializableType) -> list[str]:
    plugin_attr = find_attr(type_info.attrs, "register_plugin")
    if plugin_attr is None:
        return []

    dependencies: list[str] = []
    dependencies.extend(attr_list_arg(plugin_attr, "dependencies"))
    for raw_arg in plugin_attr.args[4:]:
        arg = raw_arg.strip()
        if not arg:
            continue
        if arg.startswith("dependency("):
            raise CodegenError(
                f"{type_info.name} uses unsupported dependency(...) plugin metadata. "
                "Pass dependency plugin names as string arguments instead."
            )

        dependencies.append(arg)
    return dependencies


def has_runtime_service_glue(type_info: SerializableType) -> bool:
    return bool(
        service_register_attrs(type_info)
        or service_register_fields(type_info)
        or fields_of(type_info, "inject")
    )


def resolve_service_factory_call(factory_method: str) -> str:
    normalized = factory_method.strip()
    if "::" in normalized or "(" in normalized:
        return normalized
    return f"tbx_value.{normalized}(tbx_services)"


def smart_ptr_value_type(field: Field, pointer_type: str) -> str:
    match = re.match(
        rf"(?:std::)?{pointer_type}\s*<\s*(.+)\s*>$",
        field.type_name.strip(),
    )
    if match is None:
        raise CodegenError(
            f"{field.name} uses [[tbx::register]] on an unsupported field type. "
            "Expected std::shared_ptr<T> or std::weak_ptr<T>."
        )
    return match.group(1).strip()


def shared_ptr_value_type(field: Field) -> str:
    return smart_ptr_value_type(field, "shared_ptr")


def weak_ptr_value_type(field: Field) -> str:
    return smart_ptr_value_type(field, "weak_ptr")


def is_shared_ptr_field(field: Field) -> bool:
    return re.match(r"(?:std::)?shared_ptr\s*<", field.type_name.strip()) is not None


def is_weak_ptr_field(field: Field) -> bool:
    return re.match(r"(?:std::)?weak_ptr\s*<", field.type_name.strip()) is not None


def registered_field_implementation_type(field: Field) -> str:
    if is_weak_ptr_field(field):
        return weak_ptr_value_type(field)
    if is_shared_ptr_field(field):
        return shared_ptr_value_type(field)
    raise CodegenError(
        f"{field.name} uses [[tbx::register]] on an unsupported field type. "
        "Expected std::shared_ptr<T> or std::weak_ptr<T>."
    )


def validate_inject_field(field: Field) -> None:
    if is_shared_ptr_field(field):
        raise CodegenError(
            "[[tbx::inject]] cannot target std::shared_ptr<T>; injected services must be "
            "weak/non-owning because strong refs can outlive plugin teardown."
        )
    if is_weak_ptr_field(field):
        return
    raise CodegenError(
        f"{field.name} uses [[tbx::inject]] on an unsupported field type. "
        "Expected std::weak_ptr<T>."
    )


def validate_inject_fields(type_info: SerializableType) -> None:
    for field in fields_of(type_info, "inject"):
        validate_inject_field(field)


def registered_field_service_type(type_info: SerializableType, field: Field) -> str:
    attr = find_attr(field.attrs, "register")
    if attr is None:
        raise CodegenError(
            f"{field.name} has register field kind without a [[tbx::register]] attribute."
        )
    service_type = attr_arg(attr, 0, "service")
    if len(attr.args) > 1 or (attr.named_args and service_type is None):
        raise CodegenError(
            f"{field.name} uses [[tbx::register]] with an invalid argument list. "
            "Expected [[tbx::register]] or [[tbx::register(ServiceType)]]."
        )
    if service_type is not None:
        return service_type
    return registered_field_implementation_type(field)


def emit_runtime_service_declarations(type_info: SerializableType) -> list[str]:
    validate_inject_fields(type_info)
    lines: list[str] = []
    if fields_of(type_info, "inject"):
        lines.append(
            f"void bind_runtime({type_info.name}& tbx_value, ::tbx::ServiceProvider& tbx_services);"
        )
    if service_register_attrs(type_info) or service_register_fields(type_info):
        lines.append(
            f"void register_services({type_info.name}& tbx_value, ::tbx::ServiceProvider& tbx_services);"
        )
    if lines:
        lines.append("")
    return lines


def emit_runtime_service_definitions(type_info: SerializableType) -> list[str]:
    validate_inject_fields(type_info)
    lines: list[str] = []
    inject_fields = fields_of(type_info, "inject")
    if inject_fields:
        lines.extend(
            [
                f"void bind_runtime({type_info.name}& tbx_value, ::tbx::ServiceProvider& tbx_services)",
                "{",
            ]
        )
        for field in inject_fields:
            lines.append(f"    ::tbx::bind_service_field(tbx_value.{field.name}, tbx_services);")
        lines.extend(["}", ""])

    register_attrs = service_register_attrs(type_info)
    register_fields = service_register_fields(type_info)
    if register_attrs or register_fields:
        lines.extend(
            [
                f"void register_services({type_info.name}& tbx_value, ::tbx::ServiceProvider& tbx_services)",
                "{",
            ]
        )
        service_index = 0
        for field in register_fields:
            implementation_type = registered_field_implementation_type(field)
            service_type = registered_field_service_type(type_info, field)
            if is_weak_ptr_field(field):
                lines.extend(
                    [
                        f"    auto tbx_service_{service_index} = tbx_value.{field.name}.lock();",
                        f"    if (!tbx_service_{service_index})",
                        "    {",
                        f"        tbx_service_{service_index} = std::make_shared<{implementation_type}>();",
                        f"        tbx_value.{field.name} = tbx_service_{service_index};",
                        "    }",
                        f"    if (tbx_service_{service_index})",
                        f"        tbx_services.register_service<{service_type}>(tbx_service_{service_index});",
                        "",
                    ]
                )
            else:
                lines.extend(
                    [
                        f"    if (!tbx_value.{field.name})",
                        f"        tbx_value.{field.name} = std::make_shared<{implementation_type}>();",
                        f"    if (tbx_value.{field.name})",
                        f"        tbx_services.register_service<{service_type}>(tbx_value.{field.name});",
                        "",
                    ]
                )
            service_index += 1
        for attr in register_attrs:
            service_type = attr_arg(attr, 0, "service")
            factory_method = attr_arg(attr, 1, "factory")
            if service_type is None or factory_method is None or len(attr.args) > 2:
                raise CodegenError(
                    f"{type_info.name} uses [[tbx::register]] with an invalid argument list. "
                    "Expected [[tbx::register(ServiceType, factory_method)]]."
                )
            factory_call = resolve_service_factory_call(factory_method)
            lines.extend(
                [
                    f"    auto tbx_service_{service_index} = {factory_call};",
                    f"    if (tbx_service_{service_index})",
                    f"        tbx_services.register_service<{service_type}>(std::move(tbx_service_{service_index}));",
                    "",
                ]
            )
            service_index += 1
        lines.extend(["}", ""])
    return lines


def emit_namespaced_runtime_service_definitions(type_info: SerializableType) -> list[str]:
    definitions = emit_runtime_service_definitions(type_info)
    if not definitions or not type_info.namespace:
        return definitions

    return [f"namespace {type_info.namespace}", "{", *definitions, "}", ""]


def resolve_custom_serialization_callable(type_info: SerializableType, callable_name: str) -> str:
    normalized = callable_name.strip()
    if "::" in normalized or "(" in normalized:
        return normalized
    if type_info.declaration_kind == "using" and type_info.namespace:
        return f"::{type_info.namespace}::{normalized}"
    return f"{type_info.name}::{normalized}"


def serializable_mode(type_info: SerializableType) -> str:
    attr = find_attr(type_info.attrs, "serializable")
    if attr is None:
        return "json"

    mode = attr_arg(attr, 0, "mode", "json") or "json"
    if mode not in {"json", "text"}:
        raise CodegenError(
            f"{type_info.name} uses unsupported serializable mode '{mode}'. "
            "Expected 'json' or 'text'."
        )
    return mode


def emit_script_asset_declarations(type_info: SerializableType, prop_fields: list[Field]) -> list[str]:
    override_helper = f"apply_script_overrides_{type_info.name}"
    bind_helper = f"bind_script_runtime_{type_info.name}"
    return [
        f"std::true_type has_asset_serialization(const {type_info.name}*);",
        *emit_json_function_declarations(type_info),
        f"::tbx::Result {override_helper}(const ::tbx::Json& tbx_json, {type_info.name}& tbx_value);",
        f"void {bind_helper}({type_info.name}& tbx_value, ::tbx::ScriptContext& tbx_context);",
        # Wraps the comma-bearing register_script_type<...> template call in a function so callers
        # (the auto-register macro, and the plugin registration) invoke it without template-arg commas.
        f"bool register_script_type_{type_info.name}();",
        "",
    ]


def emit_script_json_function_definitions(type_info: SerializableType, fields: list[Field]) -> list[str]:
    if not any(is_weak_ptr_field(field) for field in fields):
        return emit_json_function_definitions(type_info, fields)

    lines = [
        f"void serialize(::tbx::Json& tbx_json, const {type_info.name}& tbx_value)",
        "{",
    ]
    for field in fields:
        if is_weak_ptr_field(field):
            lines.extend(
                [
                    "    ::tbx::write_script_reference_field(",
                    "        tbx_json,",
                    f"        {cpp_string(json_key(field))},",
                    "        tbx_value,",
                    f"        tbx_value.{field.name});",
                ]
            )
            continue

        lines.extend(emit_typed_write_field(field))

    lines.extend(
        [
            "}",
            f"void deserialize(const ::tbx::Json& tbx_json, {type_info.name}& tbx_value)",
            "{",
        ]
    )
    if any(not is_weak_ptr_field(field) for field in fields):
        lines.append(f"    const {type_info.name} tbx_default_value {{}};")
    for field in fields:
        if is_weak_ptr_field(field):
            lines.extend(
                [
                    "    ::tbx::read_script_reference_field(",
                    "        tbx_json,",
                    f"        {cpp_string(json_key(field))},",
                    "        tbx_value,",
                    f"        tbx_value.{field.name});",
                ]
            )
            continue

        lines.extend(
            [
                "    ::tbx::read_typed_serialization_field(",
                "        tbx_json,",
                f"        {cpp_string(json_key(field))},",
                f"        tbx_value.{field.name},",
                f"        tbx_default_value.{field.name});",
            ]
        )
    lines.extend(["}", ""])
    return lines


def emit_script_asset(type_info: SerializableType, version: str, prop_fields: list[Field]) -> list[str]:
    if "Script" not in type_info.bases:
        raise CodegenError(
            f"{type_info.name} uses [[tbx::register_script]] but does not derive from tbx::Script."
        )
    validate_inject_fields(type_info)

    bind_fields = prop_fields + fields_of(type_info, "inject")
    override_helper = f"apply_script_overrides_{type_info.name}"
    bind_helper = f"bind_script_runtime_{type_info.name}"
    lines = [
        f"std::true_type has_asset_serialization(const {type_info.name}*)",
        "{",
        "    return {};",
        "}",
    ]
    lines.extend(emit_script_json_function_definitions(type_info, prop_fields))
    lines.extend(
        [
            f"::tbx::Result {override_helper}(const ::tbx::Json& tbx_json, {type_info.name}& tbx_value)",
            "{",
            "    try",
            "    {",
            "        if (!tbx_json.is_object())",
            "            return ::tbx::Result();",
        ]
    )
    for field in prop_fields:
        if is_weak_ptr_field(field):
            lines.extend(
                [
                    f"        if (const auto tbx_value_it = tbx_json.find({cpp_string(json_key(field))}); tbx_value_it != tbx_json.end())",
                    "        {",
                    "            auto tbx_reference = ::tbx::Handle {};",
                    "            ::tbx::read_typed_serialization_value(*tbx_value_it, tbx_reference);",
                    "            auto tbx_binding = ::tbx::ScriptBinding {};",
                    "            tbx_binding.script = tbx_reference.id;",
                    f"            tbx_value.set_script_reference({cpp_string(json_key(field))}, tbx_binding);",
                    f"            tbx_value.{field.name} = {{}};",
                    "        }",
                ]
            )
            continue

        lines.extend(
            [
                f"        if (const auto tbx_value_it = tbx_json.find({cpp_string(json_key(field))}); tbx_value_it != tbx_json.end())",
                f"            ::tbx::read_typed_serialization_value(*tbx_value_it, tbx_value.{field.name});",
            ]
        )
    lines.extend(
        [
            "        return ::tbx::Result();",
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
        if field in prop_fields and is_weak_ptr_field(field):
            lines.extend(
                [
                    "    ::tbx::bind_script_reference_field(",
                    "        tbx_value,",
                    f"        {cpp_string(json_key(field))},",
                    f"        tbx_value.{field.name},",
                    "        tbx_context);",
                ]
            )
            continue

        lines.append(f"    ::tbx::bind_script_field(tbx_value.{field.name}, tbx_context);")
    lines.extend(
        [
            "}",
            f"bool register_script_type_{type_info.name}()",
            "{",
            "    return ::tbx::register_script_type<",
            f"        {type_info.name},",
            f"        &{override_helper},",
            f"        &{bind_helper}>({version});",
            "}",
            "TBX_SERIALIZATION_AUTO_REGISTER(",
            "    tbx_script_asset_type_registration_,",
            f"    register_script_type_{type_info.name}());",
            "",
        ]
    )
    return lines


def emit_serialization_type(type_info: SerializableType, target: str) -> list[str]:
    version = type_version(type_info)
    prop_fields = serialized_fields(type_info)
    type_meta_attr = find_attr(type_info.attrs, "meta")
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
        custom_write_raw = attr_arg(custom_serialization_attr, 0, "write")
        custom_read_raw = attr_arg(custom_serialization_attr, 1, "read")
        if (
            custom_write_raw is None
            or custom_read_raw is None
            or len(custom_serialization_attr.args) > 2
        ):
            raise CodegenError(
                f"{type_info.name} uses [[tbx::custom_serialization]] with an invalid argument list. "
                "Expected [[tbx::custom_serialization(write_fn, read_fn)]]."
            )
        custom_write_callable = resolve_custom_serialization_callable(
            type_info,
            custom_write_raw,
        )
        custom_read_callable = resolve_custom_serialization_callable(
            type_info,
            custom_read_raw,
        )

    if has_attr(type_info.attrs, "register_script"):
        if version is None:
            raise CodegenError(f"{type_info.name} is a script and requires [[tbx::version(N)]].")
        if target == "header":
            lines.extend(emit_type_name(type_info))
            lines.extend(emit_version(type_info))
            lines.extend(emit_lifecycle_hook_declarations(type_info))
            lines.extend(emit_script_asset_declarations(type_info, prop_fields))
        else:
            lines.extend(emit_lifecycle_hook_definitions(type_info))
            lines.extend(emit_script_asset(type_info, version, prop_fields))
        return lines

    if has_attr(type_info.attrs, "serializable"):
        mode = serializable_mode(type_info)
        if type_info.template_params:
            # A template value type (e.g. AssetHandle<TAsset>) gets header-only template serialize/
            # deserialize and no runtime registration, type name, or formatter/hash glue — a template
            # is not one concrete type the registry could key on; it just serializes inline as a field.
            if mode != "json":
                raise CodegenError(f"{type_info.name} template serialization only supports json mode.")
            if type_info.declaration_kind not in {"struct", "class"}:
                raise CodegenError(f"{type_info.name} template serialization requires a struct or class.")
            if len(prop_fields) != 1:
                raise CodegenError(
                    f"{type_info.name} template serialization requires exactly one serialized field."
                )
            if target == "header":
                lines.extend(emit_template_value_serialization(type_info, prop_fields[0]))
            return lines

        if target == "header":
            lines.extend(emit_type_name(type_info))
            lines.extend(emit_version(type_info))
            lines.extend(emit_lifecycle_hook_declarations(type_info))
        else:
            lines.extend(emit_lifecycle_hook_definitions(type_info))

        if type_info.declaration_kind == "enum":
            if mode != "json":
                raise CodegenError(f"{type_info.name} enum serialization only supports json mode.")
            if target == "header":
                lines.extend(emit_enum_declarations(type_info))
            else:
                lines.extend(emit_enum(type_info))
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
                    lines.extend(emit_struct_value_serialization(type_info, prop_fields))
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
                        f"{type_info.name} text mode uses one serialized field instead of [[tbx::text]]."
                    )
                if mode == "text" and len(prop_fields) != 1:
                    raise CodegenError(
                        f"{type_info.name} text mode requires exactly one serialized field."
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
                    return lines
                if meta_fields:
                    raise CodegenError(f"{type_info.name} has [[tbx::meta]] fields but is not an Asset.")
                if text_fields:
                    raise CodegenError(f"{type_info.name} has [[tbx::text]] fields but is not an Asset.")
                if prop_fields:
                    if target == "header":
                        lines.extend(emit_struct_serialization_declarations(type_info))
                    else:
                        lines.extend(emit_struct_value_serialization(type_info, prop_fields))
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

    return lines


def default_codegen_registry() -> CodegenRegistry:
    return CodegenRegistry(
        [
            AppProcessor(),
            ServiceBindingProcessor(
                has_runtime_service_glue,
                emit_runtime_service_declarations,
                emit_runtime_service_definitions,
            ),
            SerializationProcessor(emit_serialization_type),
            FormatterProcessor(),
            HashProcessor(),
            PluginProcessor(),
        ]
    )


def emit_forward_declaration(type_info: SerializableType) -> list[str]:
    if type_info.declaration_kind == "enum" and not type_info.enum_scoped:
        return []

    # A template type is included after its own full definition (its serialize/deserialize are function
    # templates that must see the complete type), so it needs no forward declaration — and a bare
    # `struct AssetHandle;` would clash with the constrained template declaration anyway.
    if type_info.template_params:
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
    registry = default_codegen_registry()
    types = registry.active_types(types)
    registry.validate(types)
    context = CodegenContext(target="header")
    grouped: dict[str, list[SerializableType]] = {}
    global_lines: list[str] = []
    for type_info in types:
        grouped.setdefault(type_info.namespace, []).append(type_info)
        global_lines.extend(registry.global_header(type_info))

    lines = [
        GENERATED_CODE_BANNER,
        "#pragma once",
        "#include \"tbx/systems/assets/serialization.h\"",
        "#include \"tbx/types/typedefs.h\"",
        "#include <cstdint>",
        "#include <format>",
        "#include <functional>",
        "#include <memory>",
        "#include <stdexcept>",
        "#include <string>",
        "#include <string_view>",
        "#include <utility>",
        "#include <variant>",
        "#include <vector>",
        "",
    ]
    for include in registry.header_includes(types):
        lines.insert(3, include)
    lines.extend(emit_forward_declarations(types))
    for namespace, namespace_types in grouped.items():
        namespace_lines: list[str] = []
        for type_info in namespace_types:
            emitted = registry.emit_header(context, type_info)
            if emitted:
                namespace_lines.append(f"// Generated Toybox metadata glue for {type_info.name}.")
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
    registry = default_codegen_registry()
    types = registry.active_types(types)
    registry.validate(types)
    if not types:
        lines = [
            GENERATED_CODE_BANNER,
            f"// No attribute reflection glue was discovered for {header_name}.",
            "",
        ]
        return "\n".join(lines).rstrip() + "\n"

    context = CodegenContext(target="source")
    grouped: dict[str, list[SerializableType]] = {}
    global_lines: list[str] = []
    for type_info in types:
        grouped.setdefault(type_info.namespace, []).append(type_info)
        global_lines.extend(registry.global_source(type_info))

    lines = [
        GENERATED_CODE_BANNER,
        f"#include {cpp_string(include_path)}",
        f"#include {cpp_string(header_name)}",
    ]
    lines.extend(registry.source_includes(types))
    lines.append("")
    for namespace, namespace_types in grouped.items():
        namespace_lines: list[str] = []
        for type_info in namespace_types:
            emitted = registry.emit_source(context, type_info)
            if emitted:
                namespace_lines.append(f"// Generated Toybox metadata glue for {type_info.name}.")
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
    plugin_name = plugin_metadata_arg(type_info, 0, type_info.name) or type_info.name
    plugin_version = plugin_metadata_arg(type_info, 1)
    if plugin_version is None:
        raise CodegenError(
            f"{type_info.name} is a plugin and requires [[tbx::register_plugin(\"name\", \"version\", ...)]]"
        )

    category_expression, category_key = plugin_category_expression(type_info)

    priority = plugin_metadata_arg(type_info, 3, "0") or "0"
    priority = priority.removesuffix("U").removesuffix("u")
    if not priority.isdigit():
        raise CodegenError(f"{type_info.name} plugin priority must be a non-negative integer.")

    description = attr_value(type_info.attrs, "description")
    dependencies = plugin_dependencies(type_info)
    if not dependencies and category_key == "gameplay":
        dependencies = GAMEPLAY_PLUGIN_DEFAULT_DEPENDENCIES
    qualified_plugin_name = qualified_name(type_info)

    dependency_entries = ", ".join(cpp_string(dependency) for dependency in dependencies)
    dependency_initializer = "{" + dependency_entries + "}"
    validated_plugin_abi_version = validate_plugin_abi_version(plugin_abi_version)
    register_attrs = service_register_attrs(type_info)
    register_fields = service_register_fields(type_info)
    inject_fields = fields_of(type_info, "inject")

    lines = [
        GENERATED_CODE_BANNER,
        f"#include {cpp_string(include_path)}",
        "#include \"tbx/interfaces/plugin.h\"",
        "#include \"tbx/systems/assets/serialization.h\"",
        "#include \"tbx/systems/plugin_api/plugin_meta.h\"",
    ]
    if inject_fields or register_attrs or register_fields:
        lines.append("#include \"tbx/systems/scripting/service_ref.h\"")
    if script_types:
        for script_include_path in sorted(set(script_include_paths or [])):
            lines.append(f"#include {cpp_string(script_include_path)}")
    if register_fields:
        lines.append("#include <memory>")
    if register_attrs:
        lines.append("#include <utility>")
    lines.append("")
    lines.extend(emit_namespaced_runtime_service_definitions(type_info))

    if script_types or register_attrs or register_fields:
        lines.extend(
            [
                "TBX_PLUGIN_ENTRY_EXPORT void tbx_register_plugin_services(",
                "    ::tbx::Plugin* plugin,",
                "    ::tbx::ServiceProvider* service_provider)",
                "{",
                "    if (plugin == nullptr || service_provider == nullptr)",
                "        return;",
                "",
                f"    auto* typed_plugin = dynamic_cast<{qualified_plugin_name}*>(plugin);",
                "    if (typed_plugin == nullptr)",
                "        return;",
                "",
            ]
        )
        for script_type in script_types or []:
            version = type_version(script_type)
            if version is None:
                raise CodegenError(
                    f"{script_type.name} is a script and requires [[tbx::version(N)]]."
                )
            namespace_prefix = f"{script_type.namespace}::" if script_type.namespace else ""
            lines.append(
                f"    static_cast<void>({namespace_prefix}register_script_type_{script_type.name}());"
            )
        if script_types and (register_attrs or register_fields):
            lines.append("")
        if register_attrs or register_fields:
            lines.append("    ::tbx::register_runtime_services(*typed_plugin, *service_provider);")
        lines.extend(["}", ""])

    if inject_fields:
        lines.extend(
            [
                "TBX_PLUGIN_ENTRY_EXPORT void tbx_bind_plugin_runtime(",
                "    ::tbx::Plugin* plugin,",
                "    ::tbx::ServiceProvider* service_provider)",
                "{",
                "    if (plugin == nullptr || service_provider == nullptr)",
                "        return;",
                "",
                f"    auto* typed_plugin = dynamic_cast<{qualified_plugin_name}*>(plugin);",
                "    if (typed_plugin == nullptr)",
                "        return;",
                "",
                "    ::tbx::bind_runtime_fields(*typed_plugin, *service_provider);",
            ]
        )
        lines.extend(["}", ""])

    lines.extend(
        [
            "::tbx::PluginMeta tbx_get_plugin_meta()",
            "{",
            "    auto meta = ::tbx::PluginMeta {};",
            f"    meta.name = {cpp_string(plugin_name)};",
            f"    meta.version = {cpp_string(plugin_version)};",
            f"    meta.description = {cpp_string(description or '')};",
            f"    meta.dependencies = {dependency_initializer};",
            f"    meta.abi_version = {validated_plugin_abi_version}U;",
            f"    meta.category = {category_expression};",
            "    meta.linkage = ::tbx::PluginLinkage::DYNAMIC;",
            f"    meta.priority = {priority}U;",
            "    return meta;",
            "}",
            "",
            "TBX_PLUGIN_ENTRY_EXPORT ::tbx::Plugin* tbx_create_plugin()",
            "{",
            f"    return new {qualified_plugin_name}();",
            "}",
            "",
            "TBX_PLUGIN_ENTRY_EXPORT void tbx_destroy_plugin(::tbx::Plugin* plugin)",
            "{",
            "    delete plugin;",
            "}",
            "",
        ]
    )
    return lines


def generate_plugin_meta(
    type_info: SerializableType,
    plugin_abi_version: str,
    plugin_resource_directory: str | None = None,
) -> str:
    plugin_name = plugin_metadata_arg(type_info, 0, type_info.name) or type_info.name
    plugin_version = plugin_metadata_arg(type_info, 1)
    if plugin_version is None:
        raise CodegenError(
            f"{type_info.name} is a plugin and requires [[tbx::register_plugin(\"name\", \"version\", ...)]]"
        )

    _, category_key = plugin_category_expression(type_info)
    priority = plugin_metadata_arg(type_info, 3, "0") or "0"
    priority = priority.removesuffix("U").removesuffix("u")
    if not priority.isdigit():
        raise CodegenError(f"{type_info.name} plugin priority must be a non-negative integer.")

    dependencies = plugin_dependencies(type_info)
    if not dependencies and category_key == "gameplay":
        dependencies = GAMEPLAY_PLUGIN_DEFAULT_DEPENDENCIES

    # The PluginMeta sidecar is read back through the engine's serializer, which is typed-only, so it
    # is written as the same self-describing { "type", "value" } schema as every other data file.
    def typed(value: object, token: str) -> dict:
        return {"type": token, "value": value}

    metadata = {
        "name": typed(plugin_name, "string"),
        "version": typed(plugin_version, "string"),
        "description": typed(attr_value(type_info.attrs, "description") or "", "string"),
        "dependencies": typed(dependencies, "array"),
        "resource_directory": typed(plugin_resource_directory or "", "string"),
        "abi_version": typed(int(validate_plugin_abi_version(plugin_abi_version)), "int"),
        "category": typed(category_key, "plugin_category"),
        "linkage": typed("dynamic", "plugin_linkage"),
        "priority": typed(int(priority), "int"),
    }
    return json.dumps(metadata, indent=4) + "\n"


def emit_app_source(type_info: SerializableType, include_path: str) -> list[str]:
    app_attr = find_attr(type_info.attrs, "app")
    if app_attr is None:
        raise CodegenError(f"{type_info.name} is an app and requires [[tbx::app(\"name\", \"version\")]].")

    app_version = attr_arg(app_attr, 1, "version")
    if app_version is None:
        raise CodegenError(
            f"{type_info.name} is an app and requires [[tbx::app(\"name\", \"version\")]]."
        )

    qualified_app_name = qualified_name(type_info)
    return [
        GENERATED_CODE_BANNER,
        f"#include {cpp_string(include_path)}",
        "#include \"tbx/systems/app/application.h\"",
        "",
        "TBX_APP_ENTRY_EXPORT ::tbx::Application* tbx_create_app()",
        "{",
        f"    return new {qualified_app_name}();",
        "}",
        "",
        "TBX_APP_ENTRY_EXPORT void tbx_destroy_app(::tbx::Application* app)",
        "{",
        "    delete app;",
        "}",
        "",
    ]


def generate_source(
    header_name: str,
    types: list[SerializableType] | None = None,
    include_path: str | None = None,
    plugin_abi_version: str = "1",
    script_types: list[SerializableType] | None = None,
    script_include_paths: list[str] | None = None,
) -> str:
    registry = default_codegen_registry()
    plugin_processor = PluginProcessor()
    app_processor = AppProcessor()
    active_types = registry.active_types(types or [])
    registry.validate(active_types)
    app_types = [type_info for type_info in active_types if app_processor.interested(type_info)]
    plugin_types = [type_info for type_info in active_types if plugin_processor.interested(type_info)]
    if len(app_types) + len(plugin_types) > 1:
        names = ", ".join(type_info.name for type_info in app_types + plugin_types)
        raise CodegenError(
            f"Only one [[tbx::register_plugin]] or [[tbx::app]] declaration is supported per generated source: {names}."
        )

    if app_types:
        if include_path is None:
            raise CodegenError(f"{app_types[0].name} app generation requires an include path.")
        return "\n".join(emit_app_source(app_types[0], include_path)).rstrip() + "\n"

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
    return generate_type_source(header_name, active_types, include_path)


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
    output_plugin_meta_path: Path | None = None,
    plugin_resource_directory: str | None = None,
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
    if output_plugin_meta_path is not None:
        plugin_processor = PluginProcessor()
        plugin_types = [type_info for type_info in types if plugin_processor.interested(type_info)]
        if len(plugin_types) != 1:
            raise CodegenError(
                "Plugin meta generation requires exactly one [[tbx::register_plugin]] declaration."
            )
        write_if_different(
            output_plugin_meta_path,
            generate_plugin_meta(
                plugin_types[0],
                plugin_abi_version,
                plugin_resource_directory,
            ),
        )
