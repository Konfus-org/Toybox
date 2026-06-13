from __future__ import annotations

import re

from model import (
    CodegenError,
    Field,
    SerializableType,
    attr_arg,
    cpp_string,
    editor_attr_value,
    find_attr,
    has_editor_attr,
    json_key,
    split_attribute_values,
)

_OBSERVABLE_PATTERN = re.compile(r"^(?:::)?(?:tbx::)?Observable\s*<\s*(.+)\s*>$")
_WRAPPER_PATTERN = re.compile(
    r"^(?:::)?(?:std::)?(?:vector|weak_ptr|shared_ptr|unique_ptr|optional)\s*<\s*(.+)\s*>$"
)
_MAP_PATTERN = re.compile(r"^(?:::)?(?:std::)?(?:unordered_map|map)\s*<\s*(.+)\s*>$")


def _unwrap_field_type(type_name: str) -> str:
    """Peels Observable/vector/map/smart-pointer/optional wrappers off a field type to reach the inner
    (element / mapped / referenced / nested) type whose reflection record describe-time enrichment
    recurses into. For a keyed container the mapped (value) type is the one that carries reflection."""
    current = type_name.strip()
    while True:
        observable = _OBSERVABLE_PATTERN.match(current)
        if observable is not None:
            arguments = split_attribute_values(observable.group(1))
            current = arguments[-1].strip() if arguments else current
            continue
        keyed = _MAP_PATTERN.match(current)
        if keyed is not None:
            arguments = split_attribute_values(keyed.group(1))
            if len(arguments) >= 2:
                current = arguments[1].strip()
                continue
        wrapper = _WRAPPER_PATTERN.match(current)
        if wrapper is not None:
            current = wrapper.group(1).strip()
            continue
        return current


def _wire_type_name(type_name: str) -> str:
    """Snake-cases a C++ type name to its serialization wire name, mirroring the engine's
    make_serializable_type_name so nested_type_name matches the nested type's registered record."""
    namespace_position = type_name.rfind("::")
    if namespace_position != -1:
        type_name = type_name[namespace_position + 2 :]

    result: list[str] = []
    for index, character in enumerate(type_name):
        if "A" <= character <= "Z":
            has_previous = index > 0
            next_is_lower = index + 1 < len(type_name) and "a" <= type_name[index + 1] <= "z"
            previous = type_name[index - 1] if has_previous else ""
            previous_is_lower_or_digit = has_previous and (
                ("a" <= previous <= "z") or ("0" <= previous <= "9")
            )
            if result and (previous_is_lower_or_digit or next_is_lower):
                result.append("_")
            result.append(character.lower())
        else:
            result.append(character)
    return "".join(result)


def emit_typed_write_field(field: Field) -> list[str]:
    """Emits one write_typed_serialization_field call. Editor metadata ([[tbx::category/description/
    view]] and the [[tbx::readonly/hidden]] flags) is no longer threaded through serialization output;
    it now lives in the reflection registry (see emit_reflection_registration)."""
    return [
        "    ::tbx::write_typed_serialization_field(",
        "        tbx_json,",
        f"        {cpp_string(json_key(field))},",
        f"        tbx_value.{field.name});",
    ]


def _emit_reflection_property(type_info: SerializableType, field: Field) -> list[str]:
    """Emits one PropertyReflection initializer for a [[prop]] field. The type token and default value
    are read from a default-constructed probe's serialized JSON, and get/set route through the owning
    type's serialize/deserialize — so private [[prop]] fields are never named directly."""
    name = type_info.name
    key = json_key(field)
    key_literal = cpp_string(key)
    category = editor_attr_value(field.attrs, "category")
    description = editor_attr_value(field.attrs, "description")
    view = editor_attr_value(field.attrs, "view")
    readonly = has_editor_attr(field.attrs, "readonly")
    hidden = has_editor_attr(field.attrs, "hidden")

    nested_type_name = _wire_type_name(_unwrap_field_type(field.type_name))

    lines = [
        "    {",
        "        auto tbx_property = ::tbx::PropertyReflection {};",
        f"        tbx_property.name = {key_literal};",
        f"        tbx_property.nested_type_name = {cpp_string(nested_type_name)};",
        "        if (tbx_has_probe)",
        "        {",
        f"            const auto tbx_field = tbx_probe_json.find({key_literal});",
        "            if (tbx_field != tbx_probe_json.end() && tbx_field->is_object())",
        "            {",
        "                if (const auto tbx_token = tbx_field->find(\"type\");",
        "                    tbx_token != tbx_field->end() && tbx_token->is_string())",
        "                    tbx_property.type_token = tbx_token->get<std::string>();",
        "                if (const auto tbx_value = tbx_field->find(\"value\");",
        "                    tbx_value != tbx_field->end())",
        "                {",
        "                    tbx_property.default_value = *tbx_value;",
        "                    tbx_property.has_default = true;",
        "                }",
        "            }",
        "        }",
    ]
    if category is not None:
        lines.append(f"        tbx_property.category = {cpp_string(category)};")
    if description is not None:
        lines.append(f"        tbx_property.description = {cpp_string(description)};")
    if view is not None:
        lines.append(f"        tbx_property.view = {cpp_string(view)};")
    if readonly:
        lines.append("        tbx_property.readonly = true;")
    if hidden:
        lines.append("        tbx_property.hidden = true;")
    lines.extend(
        [
            f"        tbx_property.get_value = ::tbx::make_property_getter<{name}>({key_literal});",
            f"        tbx_property.set_value = ::tbx::make_property_setter<{name}>({key_literal});",
            "        tbx_record.properties.push_back(std::move(tbx_property));",
            "    }",
        ]
    )
    return lines


def emit_reflection_registration(
    type_info: SerializableType,
    prop_fields: list[Field],
) -> list[str]:
    """Emits the runtime reflection record for a type's [[prop]] fields plus its static-init registrar.
    The builder has external linkage so it is not flagged unused when the registrar no-ops in plugins."""
    name = type_info.name
    lines = [
        f"::tbx::TypeReflection tbx_build_type_reflection_{name}()",
        "{",
        "    auto tbx_record = ::tbx::TypeReflection {};",
        "    tbx_record.name = ::tbx::make_serializable_type_name(",
        f"        tbx_serialization_type_name(static_cast<const {name}*>(nullptr)));",
        f"    tbx_record.type_name = {cpp_string(name)};",
        f"    tbx_record.type = std::type_index(typeid({name}));",
        "    {",
        f"        const auto tbx_icon = ::tbx::get_property_type_icon<{name}>();",
        "        tbx_record.icon = std::string(tbx_icon.name);",
        "        tbx_record.icon_color = std::string(tbx_icon.color);",
        "    }",
        # A default-constructed probe, serialized once, supplies each property's type token and default
        # value without naming members (so private [[prop]] fields are handled too).
        f"    constexpr bool tbx_has_probe = std::is_default_constructible_v<{name}>;",
        "    auto tbx_probe_json = ::tbx::Json();",
        f"    if constexpr (std::is_default_constructible_v<{name}>)",
        f"        tbx_probe_json = ::tbx::write_serialization_value<::tbx::Json>({name} {{}});",
    ]
    for field in prop_fields:
        lines.extend(_emit_reflection_property(type_info, field))
    lines.extend(
        [
            "    return tbx_record;",
            "}",
            "TBX_REFLECTION_AUTO_REGISTER(",
            "    tbx_type_reflection_registration_,",
            f"    ::tbx::register_type_reflection<{name}>(&tbx_build_type_reflection_{name}));",
            "",
        ]
    )
    return lines


def emit_property_icon_declaration(type_info: SerializableType) -> list[str]:
    """Forward-declares the type's tbx_property_type_icon overload in the generated header so any
    translation unit serializing a field of this type can advertise its [[tbx::icon]]."""
    if find_attr(type_info.attrs, "icon") is None:
        return []
    return [f"::tbx::PropertyTypeIcon tbx_property_type_icon(const {type_info.name}*);"]


def emit_property_icon_overload(type_info: SerializableType) -> list[str]:
    """Defines the tbx_property_type_icon overload for a type tagged [[tbx::icon("Name", Color::X)]], so
    write_typed_serialization_field advertises the type's editor icon. Returns [] when absent."""
    attr = find_attr(type_info.attrs, "icon")
    if attr is None:
        return []

    name = attr_arg(attr, 0, "name")
    if not name:
        raise CodegenError(f"{type_info.name} uses [[tbx::icon]] without an icon name.")

    # The colour argument is a Color constant (e.g. Color::BLUE); the editor keys off the bare name.
    color = attr_arg(attr, 1, "color") or ""
    color = color.rsplit("::", 1)[-1].strip()
    return [
        f"::tbx::PropertyTypeIcon tbx_property_type_icon(const {type_info.name}*)",
        "{",
        f"    return {{ {cpp_string(name)}, {cpp_string(color)} }};",
        "}",
        "",
    ]


def emit_lifecycle_hook_declarations(type_info: SerializableType) -> list[str]:
    hook_definitions = [
        ("pre_serialize", "pre_serialize", True),
        ("post_serialize", "post_serialize", True),
        ("pre_deserialize", "pre_deserialize", False),
        ("post_deserialize", "post_deserialize", False),
    ]

    lines: list[str] = []
    for attribute_name, hook_name, is_const in hook_definitions:
        attribute = find_attr(type_info.attrs, attribute_name)
        if attribute is None:
            continue
        callable_name = attr_arg(attribute, 0, "method")
        if callable_name is None or len(attribute.args) > 1:
            raise CodegenError(
                f"{type_info.name} uses [[tbx::{attribute_name}]] with an invalid argument list."
            )

        qualifier = "const " if is_const else ""
        lines.append(f"void {hook_name}({qualifier}{type_info.name}& tbx_value);")

    if lines:
        lines.append("")
    return lines


def emit_lifecycle_hook_definitions(type_info: SerializableType) -> list[str]:
    hook_definitions = [
        ("pre_serialize", "pre_serialize", True),
        ("post_serialize", "post_serialize", True),
        ("pre_deserialize", "pre_deserialize", False),
        ("post_deserialize", "post_deserialize", False),
    ]

    lines: list[str] = []
    for attribute_name, hook_name, is_const in hook_definitions:
        attribute = find_attr(type_info.attrs, attribute_name)
        if attribute is None:
            continue
        callable_name = attr_arg(attribute, 0, "method")
        if callable_name is None or len(attribute.args) > 1:
            raise CodegenError(
                f"{type_info.name} uses [[tbx::{attribute_name}]] with an invalid argument list."
            )

        callable_name = callable_name.strip()
        qualifier = "const " if is_const else ""
        lines.extend([f"void {hook_name}({qualifier}{type_info.name}& tbx_value)", "{"])
        if "::" in callable_name or "(" in callable_name:
            lines.append(f"    {callable_name}(tbx_value);")
        else:
            lines.append(f"    tbx_value.{callable_name}();")
        lines.extend(["}", ""])

    return lines


def emit_json_function_declarations(type_info: SerializableType) -> list[str]:
    source_path = type_info.source_path.replace("\\", "/")
    default_api_macro = (
        "TBX_API"
        if type_info.namespace == "tbx" and "/engine/include/" in source_path
        else ""
    )
    api_macro = type_info.api_macro or default_api_macro
    api_prefix = f"{api_macro} " if api_macro else ""
    return [
        f"{api_prefix}void serialize(::tbx::Json& tbx_json, const {type_info.name}& tbx_value);",
        f"{api_prefix}void deserialize(const ::tbx::Json& tbx_json, {type_info.name}& tbx_value);",
        "",
    ]


def emit_json_function_definitions(type_info: SerializableType, fields: list[Field]) -> list[str]:
    if not fields:
        return [
            f"void serialize(::tbx::Json& tbx_json, const {type_info.name}&)",
            "{",
            "    tbx_json = ::tbx::Json::object();",
            "}",
            f"void deserialize(const ::tbx::Json&, {type_info.name}&)",
            "{",
            "}",
            "",
        ]

    lines = [
        f"void serialize(::tbx::Json& tbx_json, const {type_info.name}& tbx_value)",
        "{",
    ]
    if len(fields) == 1:
        field = fields[0]
        lines.extend(
            [
                "    tbx_json = ::tbx::write_serialization_value<::tbx::Json>(",
                f"        tbx_value.{field.name});",
                "}",
                f"void deserialize(const ::tbx::Json& tbx_json, {type_info.name}& tbx_value)",
                "{",
                f"    const {type_info.name} tbx_default_value {{}};",
                "    if (tbx_json.is_object() || tbx_json.is_null())",
                "    {",
                "        ::tbx::read_serialization_field(",
                "            tbx_json,",
                f"            {cpp_string(json_key(field))},",
                f"            tbx_value.{field.name},",
                f"            tbx_default_value.{field.name});",
                "        return;",
                "    }",
                "",
                "    ::tbx::read_serialization_value(",
                "        tbx_json,",
                f"        tbx_value.{field.name});",
                "}",
                "",
            ]
        )
        return lines

    for field in fields:
        lines.extend(emit_typed_write_field(field))
    lines.extend(
        [
            "}",
            f"void deserialize(const ::tbx::Json& tbx_json, {type_info.name}& tbx_value)",
            "{",
            f"    const {type_info.name} tbx_default_value {{}};",
        ]
    )
    for field in fields:
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


def emit_struct_serialization_declarations(
    type_info: SerializableType,
    needs_json: bool = True,
) -> list[str]:
    source_path = type_info.source_path.replace("\\", "/")
    default_api_macro = (
        "TBX_API"
        if type_info.namespace == "tbx" and "/engine/include/" in source_path
        else ""
    )
    api_macro = type_info.api_macro or default_api_macro
    api_prefix = f"{api_macro} " if api_macro else ""
    lines = [
        f"std::true_type tbx_has_struct_serialization(const {type_info.name}*);",
        f"{api_prefix}bool tbx_register_serializable_type(const {type_info.name}*);",
    ]
    lines.extend(emit_property_icon_declaration(type_info))
    if needs_json:
        lines.extend(emit_json_function_declarations(type_info)[:-1])
    lines.append("")
    return lines


def emit_struct_trait_definition(type_info: SerializableType) -> list[str]:
    return [
        f"std::true_type tbx_has_struct_serialization(const {type_info.name}*)",
        "{",
        "    return {};",
        "}",
        "",
    ]


def emit_serializable_registration(type_info: SerializableType, custom: bool = False) -> list[str]:
    if custom:
        return (
            emit_struct_trait_definition(type_info)
            + [
                f"void serialize(::tbx::Json& tbx_json, const {type_info.name}& tbx_value)",
                "{",
                f"    tbx_json = ::tbx::Json::parse(::tbx::Serializer<{type_info.name}>::serialize(tbx_value));",
                "}",
                f"void deserialize(const ::tbx::Json& tbx_json, {type_info.name}& tbx_value)",
                "{",
                f"    if (!::tbx::Serializer<{type_info.name}>::deserialize(tbx_json.dump(), tbx_value))",
                "        throw std::runtime_error(\"Failed to parse custom Toybox serializable type.\");",
                "}",
                f"bool tbx_register_serializable_type(const {type_info.name}*)",
                "{",
                f"    return ::tbx::register_serializable_type<{type_info.name}>();",
                "}",
                "TBX_SERIALIZATION_AUTO_REGISTER(",
                "    tbx_serializable_type_registration_,",
                f"    tbx_register_serializable_type(static_cast<const {type_info.name}*>(nullptr)));",
                "",
            ]
            + emit_property_icon_overload(type_info)
        )

    return (
        emit_struct_trait_definition(type_info)
        + [
            f"std::string tbx_write_json_serializable_value(const {type_info.name}& tbx_serialization_value)",
            "{",
            "    pre_serialize(tbx_serialization_value);",
            "    auto tbx_serialization_json = ::tbx::Json();",
            "    serialize(tbx_serialization_json, tbx_serialization_value);",
            "    const auto tbx_serialization_data = tbx_serialization_json.dump();",
            "    post_serialize(tbx_serialization_value);",
            "    return tbx_serialization_data;",
            "}",
            "bool tbx_read_json_serializable_value(",
            "    std::string_view tbx_serialization_data,",
            f"    {type_info.name}& tbx_serialization_value)",
            "{",
            "    try",
            "    {",
            "        pre_deserialize(tbx_serialization_value);",
            "        deserialize(",
            "            ::tbx::JsonParser::parse(tbx_serialization_data),",
            "            tbx_serialization_value);",
            "        post_deserialize(tbx_serialization_value);",
            "        return true;",
            "    }",
            "    catch (...)",
            "    {",
            "        return false;",
            "    }",
            "}",
            f"bool tbx_register_serializable_type(const {type_info.name}*)",
            "{",
            f"    return ::tbx::register_serializable_type<{type_info.name}>(",
            f"        [](const {type_info.name}& tbx_serialization_value)",
            "        {",
            "            return tbx_write_json_serializable_value(tbx_serialization_value);",
            "        },",
            f"        [](std::string_view tbx_serialization_data, {type_info.name}& tbx_serialization_value)",
            "        {",
            "            return tbx_read_json_serializable_value(",
            "                tbx_serialization_data,",
            "                tbx_serialization_value);",
            "        });",
            "}",
            "TBX_SERIALIZATION_AUTO_REGISTER(",
            "    tbx_serializable_type_registration_,",
            f"    tbx_register_serializable_type(static_cast<const {type_info.name}*>(nullptr)));",
            "",
        ]
        + emit_property_icon_overload(type_info)
    )


def emit_custom_serializable_registration(
    type_info: SerializableType,
    write_callable: str,
    read_callable: str,
) -> list[str]:
    return (
        emit_struct_trait_definition(type_info)
        + [
            f"void serialize(::tbx::Json& tbx_json, const {type_info.name}& tbx_value)",
            "{",
            f"    tbx_json = ::tbx::Json::parse({write_callable}(tbx_value));",
            "}",
            f"void deserialize(const ::tbx::Json& tbx_json, {type_info.name}& tbx_value)",
            "{",
            f"    if (!{read_callable}(tbx_json.dump(), tbx_value))",
            "        throw std::runtime_error(\"Failed to parse custom Toybox serializable type.\");",
            "}",
            f"std::string tbx_write_json_serializable_value(const {type_info.name}& tbx_serialization_value)",
            "{",
            "    pre_serialize(tbx_serialization_value);",
            f"    const auto tbx_serialization_data = {write_callable}(tbx_serialization_value);",
            "    post_serialize(tbx_serialization_value);",
            "    return tbx_serialization_data;",
            "}",
            "bool tbx_read_json_serializable_value(",
            "    std::string_view tbx_serialization_data,",
            f"    {type_info.name}& tbx_serialization_value)",
            "{",
            "    pre_deserialize(tbx_serialization_value);",
            f"    if (!{read_callable}(tbx_serialization_data, tbx_serialization_value))",
            "        return false;",
            "    post_deserialize(tbx_serialization_value);",
            "    return true;",
            "}",
            f"bool tbx_register_serializable_type(const {type_info.name}*)",
            "{",
            f"    return ::tbx::register_serializable_type<{type_info.name}>(",
            f"        [](const {type_info.name}& tbx_serialization_value)",
            "        {",
            "            return tbx_write_json_serializable_value(tbx_serialization_value);",
            "        },",
            f"        [](std::string_view tbx_serialization_data, {type_info.name}& tbx_serialization_value)",
            "        {",
            "            return tbx_read_json_serializable_value(",
            "                tbx_serialization_data,",
            "                tbx_serialization_value);",
            "        });",
            "}",
            "TBX_SERIALIZATION_AUTO_REGISTER(",
            "    tbx_serializable_type_registration_,",
            f"    tbx_register_serializable_type(static_cast<const {type_info.name}*>(nullptr)));",
            "",
        ]
    )


def emit_indexed(type_info: SerializableType, count: str) -> list[str]:
    return [
        f"void serialize(::tbx::Json& tbx_json, const {type_info.name}& tbx_value)",
        "{",
        "    tbx_json = ::tbx::write_indexed_serialization_value<::tbx::Json>(",
        "        tbx_value,",
        f"        {count});",
        "}",
        f"void deserialize(const ::tbx::Json& tbx_json, {type_info.name}& tbx_value)",
        "{",
        "    ::tbx::read_indexed_serialization_value(",
        "        tbx_json,",
        "        tbx_value,",
        f"        {count});",
        "}",
        "",
    ] + emit_serializable_registration(type_info)
