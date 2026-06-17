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


def _attribute_descriptor_literal(field: Field, order: int) -> str:
    """Builds a ::tbx::PropertyAttributeInfo literal from a field's editor attributes. It is passed to the
    attribute-aware write so the metadata is emitted inline next to the value when attribute serialization
    is on. The type token and enum choices are derived from the field's static type at write time; only the
    editor attributes ([[tbx::category/description/view/readonly/hidden]]), the nested wire-type name
    (so the editor can tell e.g. a quaternion from a plain vec4), and the field's declaration order (so the
    editor lists properties in source order, not the alphabetical key order the JSON map imposes) are baked
    here."""
    parts: list[str] = []
    category = editor_attr_value(field.attrs, "category")
    if category is not None:
        parts.append(f".category = {cpp_string(category)}")
    description = editor_attr_value(field.attrs, "description")
    if description is not None:
        parts.append(f".description = {cpp_string(description)}")
    view = editor_attr_value(field.attrs, "view")
    if view is not None:
        parts.append(f".view = {cpp_string(view)}")
    label = editor_attr_value(field.attrs, "label")
    if label is not None:
        parts.append(f".label = {cpp_string(label)}")
    nested = _wire_type_name(_unwrap_field_type(field.type_name))
    if nested:
        parts.append(f".nested = {cpp_string(nested)}")
    if has_editor_attr(field.attrs, "readonly"):
        parts.append(".readonly = true")
    if has_editor_attr(field.attrs, "hidden"):
        parts.append(".hidden = true")
    # order is the last field of PropertyAttributeInfo; designated initializers must follow declaration
    # order, so it is appended last.
    parts.append(f".order = {order}")
    return "::tbx::PropertyAttributeInfo { " + ", ".join(parts) + " }"


def emit_typed_write_field(field: Field, order: int, with_default: bool = False) -> list[str]:
    """Emits one write_typed_serialization_field call. The multi-field (with_default) path passes the
    field's editor attributes so attribute serialization can emit them inline next to the value; the same
    call stays lean and omits defaults on the persistence path. The caller must have declared
    `const T tbx_default_value {};` in scope. `order` is the field's declaration index, baked so the editor
    can present properties in source order."""
    if with_default:
        return [
            "    ::tbx::write_typed_serialization_field(",
            "        tbx_json,",
            f"        {cpp_string(json_key(field))},",
            f"        tbx_value.{field.name},",
            f"        tbx_default_value.{field.name},",
            f"        {_attribute_descriptor_literal(field, order)});",
        ]
    return [
        "    ::tbx::write_typed_serialization_field(",
        "        tbx_json,",
        f"        {cpp_string(json_key(field))},",
        f"        tbx_value.{field.name});",
    ]


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

    # Multi-field structs serialize with default omission: a default-constructed probe supplies each
    # field's default, and the four-argument write skips fields equal to it when the per-thread omit
    # switch is on (the persistence path). The probe mirrors the one deserialize already builds.
    lines.append(f"    const {type_info.name} tbx_default_value {{}};")
    for order, field in enumerate(fields):
        lines.extend(emit_typed_write_field(field, order, with_default=True))
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
