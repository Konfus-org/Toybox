from __future__ import annotations

import re

from model import (
    CodegenError,
    Field,
    SerializableType,
    attr_arg,
    cpp_string,
    find_attr,
    json_key,
    split_attribute_values,
)

_OBSERVABLE_PATTERN = re.compile(r"^(?:::)?(?:tbx::)?Observable\s*<\s*(.+)\s*>$")
_CLAMP_PATTERN = re.compile(r"^(?:::)?(?:tbx::)?Clamp\s*<\s*(.+)\s*>$")
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
        clamp = _CLAMP_PATTERN.match(current)
        if clamp is not None:
            # Clamp<T, Min, Max> carries its editable value in the first argument; the bounds are
            # compile-time only, so the wire/editor type is just T.
            arguments = split_attribute_values(clamp.group(1))
            current = arguments[0].strip() if arguments else current
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


def attribute_descriptor_literal(field: Field, order: int) -> str:
    """Builds a ::tbx::PropertyAttributeInfo literal for a field. It is passed to the attribute-aware
    write so the metadata is emitted inline next to the value when attribute serialization is on. The
    type token and enum choices are derived from the field's static type at write time; only the nested
    wire-type name (so the editor can tell e.g. a quaternion from a plain vec4) and the field's
    declaration order (so the editor lists properties in source order, not the alphabetical key order
    the JSON map imposes) are baked here."""
    parts: list[str] = []
    nested = _wire_type_name(_unwrap_field_type(field.type_name))
    if nested:
        parts.append(f".nested = {cpp_string(nested)}")
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
            f"        {attribute_descriptor_literal(field, order)});",
        ]
    return [
        "    ::tbx::write_typed_serialization_field(",
        "        tbx_json,",
        f"        {cpp_string(json_key(field))},",
        f"        tbx_value.{field.name});",
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


def emit_json_function_definitions(
    type_info: SerializableType,
    fields: list[Field],
    force_keyed: bool = False,
) -> list[str]:
    """Emits the serialize/deserialize pair. A single-field value type serializes as its bare field
    value (the fast path components like Tag rely on); pass force_keyed for types whose body must
    stay a keyed document regardless of field count (asset files, which editors read per-field)."""
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
    if len(fields) == 1 and not force_keyed:
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
                # The keyed form is only produced by hand-authored/legacy files (the fast path above
                # writes the bare field value). Its field carries the self-describing { "type",
                # "value" } wrapper, so it must be unwrapped exactly like a multi-field struct — a
                # plain read_serialization_field would hand the wrapper object straight to the field's
                # deserialize, which for an enum/typed field silently falls back to its default.
                "        ::tbx::read_typed_serialization_field(",
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
    if len(fields) == 1:
        # A force_keyed single-field type once serialized as its bare field value; keep reading
        # that legacy shape so existing files stay loadable.
        field = fields[0]
        lines.extend(
            [
                "    if (!tbx_json.is_object() && !tbx_json.is_null())",
                "    {",
                "        ::tbx::read_serialization_value(",
                "            tbx_json,",
                f"            tbx_value.{field.name});",
                "        return;",
                "    }",
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


def emit_access_broker_serialization(type_info: SerializableType, fields: list[Field]) -> list[str]:
    """Emit serialize/deserialize for a type with non-public serialized members. The bodies live in a
    ``::tbx::SerializationAccess<T>`` specialization (befriended in-class via TBX_EXPOSE_PRIVATES_TO_SERIALIZATION,
    so it can touch private members); the free serialize/deserialize functions delegate to it. The
    specialization is defined in the generated source where the type is complete, and — because it
    specializes ``::tbx::SerializationAccess`` — it must be emitted inside ``namespace tbx``."""
    if type_info.namespace != "tbx":
        raise CodegenError(
            f"{type_info.name} has non-public [[tbx::serialize]] members, which are currently only "
            "supported for types in namespace tbx."
        )

    body_functions = emit_json_function_definitions(type_info, fields)
    specialization_body: list[str] = []
    for line in body_functions:
        if line.startswith("void serialize(") or line.startswith("void deserialize("):
            specialization_body.append(f"    static {line}")
        elif line:
            specialization_body.append(f"    {line}")
        else:
            specialization_body.append(line)

    return [
        "template <>",
        f"struct SerializationAccess<{type_info.name}>",
        "{",
        *specialization_body,
        "};",
        "",
        f"void serialize(::tbx::Json& tbx_json, const {type_info.name}& tbx_value)",
        "{",
        f"    ::tbx::SerializationAccess<{type_info.name}>::serialize(tbx_json, tbx_value);",
        "}",
        f"void deserialize(const ::tbx::Json& tbx_json, {type_info.name}& tbx_value)",
        "{",
        f"    ::tbx::SerializationAccess<{type_info.name}>::deserialize(tbx_json, tbx_value);",
        "}",
        "",
    ]


def emit_struct_value_serialization(type_info: SerializableType, fields: list[Field]) -> list[str]:
    """Emit serialize/deserialize for a value struct, routing through the access broker when any
    serialized member is non-public, otherwise emitting plain free functions."""
    from model import non_public_serialized_fields

    if non_public_serialized_fields(type_info):
        return emit_access_broker_serialization(type_info, fields)
    return emit_json_function_definitions(type_info, fields)


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
        f"std::true_type has_struct_serialization(const {type_info.name}*);",
        f"{api_prefix}bool register_serializable_type(const {type_info.name}*);",
    ]
    if needs_json:
        lines.extend(emit_json_function_declarations(type_info)[:-1])
    lines.append("")
    return lines


def emit_struct_trait_definition(type_info: SerializableType) -> list[str]:
    return [
        f"std::true_type has_struct_serialization(const {type_info.name}*)",
        "{",
        "    return {};",
        "}",
        "",
    ]


def emit_serializable_registration(type_info: SerializableType, custom: bool = False) -> list[str]:
    if custom:
        return emit_struct_trait_definition(type_info) + [
            f"void serialize(::tbx::Json& tbx_json, const {type_info.name}& tbx_value)",
            "{",
            f"    tbx_json = ::tbx::Json::parse(::tbx::Serializer<{type_info.name}>::serialize(tbx_value));",
            "}",
            f"void deserialize(const ::tbx::Json& tbx_json, {type_info.name}& tbx_value)",
            "{",
            f"    if (!::tbx::Serializer<{type_info.name}>::deserialize(tbx_json.dump(), tbx_value))",
            "        throw std::runtime_error(\"Failed to parse custom Toybox serializable type.\");",
            "}",
            f"bool register_serializable_type(const {type_info.name}*)",
            "{",
            f"    return ::tbx::register_serializable_type<{type_info.name}>();",
            "}",
            "TBX_SERIALIZATION_AUTO_REGISTER(",
            "    tbx_serializable_type_registration_,",
            f"    register_serializable_type(static_cast<const {type_info.name}*>(nullptr)));",
            "",
        ]

    return emit_struct_trait_definition(type_info) + [
        f"std::string write_json_serializable_value(const {type_info.name}& tbx_serialization_value)",
        "{",
        "    pre_serialize(tbx_serialization_value);",
        "    auto tbx_serialization_json = ::tbx::Json();",
        "    serialize(tbx_serialization_json, tbx_serialization_value);",
        "    const auto tbx_serialization_data = tbx_serialization_json.dump();",
        "    post_serialize(tbx_serialization_value);",
        "    return tbx_serialization_data;",
        "}",
        "bool read_json_serializable_value(",
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
        f"bool register_serializable_type(const {type_info.name}*)",
        "{",
        f"    return ::tbx::register_serializable_type<{type_info.name}>(",
        f"        [](const {type_info.name}& tbx_serialization_value)",
        "        {",
        "            return write_json_serializable_value(tbx_serialization_value);",
        "        },",
        f"        [](std::string_view tbx_serialization_data, {type_info.name}& tbx_serialization_value)",
        "        {",
        "            return read_json_serializable_value(",
        "                tbx_serialization_data,",
        "                tbx_serialization_value);",
        "        });",
        "}",
        "TBX_SERIALIZATION_AUTO_REGISTER(",
        "    tbx_serializable_type_registration_,",
        f"    register_serializable_type(static_cast<const {type_info.name}*>(nullptr)));",
        "",
    ]


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
            f"std::string write_json_serializable_value(const {type_info.name}& tbx_serialization_value)",
            "{",
            "    pre_serialize(tbx_serialization_value);",
            f"    const auto tbx_serialization_data = {write_callable}(tbx_serialization_value);",
            "    post_serialize(tbx_serialization_value);",
            "    return tbx_serialization_data;",
            "}",
            "bool read_json_serializable_value(",
            "    std::string_view tbx_serialization_data,",
            f"    {type_info.name}& tbx_serialization_value)",
            "{",
            "    pre_deserialize(tbx_serialization_value);",
            f"    if (!{read_callable}(tbx_serialization_data, tbx_serialization_value))",
            "        return false;",
            "    post_deserialize(tbx_serialization_value);",
            "    return true;",
            "}",
            f"bool register_serializable_type(const {type_info.name}*)",
            "{",
            f"    return ::tbx::register_serializable_type<{type_info.name}>(",
            f"        [](const {type_info.name}& tbx_serialization_value)",
            "        {",
            "            return write_json_serializable_value(tbx_serialization_value);",
            "        },",
            f"        [](std::string_view tbx_serialization_data, {type_info.name}& tbx_serialization_value)",
            "        {",
            "            return read_json_serializable_value(",
            "                tbx_serialization_data,",
            "                tbx_serialization_value);",
            "        });",
            "}",
            "TBX_SERIALIZATION_AUTO_REGISTER(",
            "    tbx_serializable_type_registration_,",
            f"    register_serializable_type(static_cast<const {type_info.name}*>(nullptr)));",
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
