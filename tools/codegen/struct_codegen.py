from __future__ import annotations

from model import CodegenError, Field, SerializableType, attr_arg, cpp_string, find_attr, json_key


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
        lines.extend(
            [
                "    ::tbx::write_serialization_field(",
                "        tbx_json,",
                f"        {cpp_string(json_key(field))},",
                f"        tbx_value.{field.name});",
            ]
        )
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
                "    ::tbx::read_serialization_field(",
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
