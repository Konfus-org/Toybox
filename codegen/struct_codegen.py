from __future__ import annotations

from model import Field, SerializableType, cpp_string, json_key


def emit_json_functions(type_info: SerializableType, fields: list[Field]) -> list[str]:
    lines = [
        "template <typename BasicJsonType>",
        f"void to_json(BasicJsonType& tbx_json, const {type_info.name}& tbx_value)",
        "{",
    ]
    for field in fields:
        lines.append(f"    tbx_json[{cpp_string(json_key(field))}] = tbx_value.{field.name};")
    lines.extend(
        [
            "}",
            "template <typename BasicJsonType>",
            f"void from_json(const BasicJsonType& tbx_json, {type_info.name}& tbx_value)",
            "{",
            f"    const {type_info.name} tbx_default_value {{}};",
        ]
    )
    for field in fields:
        lines.extend(
            [
                "    ::tbx::internal::read_serialization_field(",
                "        tbx_json,",
                f"        {cpp_string(json_key(field))},",
                f"        tbx_value.{field.name},",
                f"        tbx_default_value.{field.name});",
            ]
        )
    lines.extend(["}", ""])
    return lines


def emit_serializable_registration(type_info: SerializableType, custom: bool = False) -> list[str]:
    if custom:
        return [
            f"inline std::true_type tbx_has_struct_serialization(const {type_info.name}*)",
            "{",
            "    return {};",
            "}",
            "template <typename BasicJsonType>",
            f"void to_json(BasicJsonType& tbx_json, const {type_info.name}& tbx_value)",
            "{",
            f"    tbx_json = BasicJsonType::parse(::tbx::Serializer<{type_info.name}>::to_json(tbx_value));",
            "}",
            "template <typename BasicJsonType>",
            f"void from_json(const BasicJsonType& tbx_json, {type_info.name}& tbx_value)",
            "{",
            f"    if (!::tbx::Serializer<{type_info.name}>::from_json(tbx_json.dump(), tbx_value))",
            "        throw std::runtime_error(\"Failed to parse custom Toybox serializable type.\");",
            "}",
            f"inline bool tbx_register_serializable_type(const {type_info.name}*)",
            "{",
            f"    return ::tbx::internal::register_serializable_type<{type_info.name}>();",
            "}",
            "TBX_INTERNAL_AUTO_REGISTER(",
            "    tbx_serializable_type_registration_,",
            f"    tbx_register_serializable_type(static_cast<const {type_info.name}*>(nullptr)));",
            "",
        ]

    return [
        f"inline std::true_type tbx_has_struct_serialization(const {type_info.name}*)",
        "{",
        "    return {};",
        "}",
        f"inline std::string tbx_write_json_serializable_value(const {type_info.name}& tbx_serialization_value)",
        "{",
        "    auto tbx_serialization_json = ::tbx::Json();",
        "    to_json(tbx_serialization_json, tbx_serialization_value);",
        "    return tbx_serialization_json.dump();",
        "}",
        "inline bool tbx_read_json_serializable_value(",
        "    std::string_view tbx_serialization_data,",
        f"    {type_info.name}& tbx_serialization_value)",
        "{",
        "    try",
        "    {",
        "        from_json(",
        "            ::tbx::JsonParser::parse(tbx_serialization_data),",
        "            tbx_serialization_value);",
        "        return true;",
        "    }",
        "    catch (...)",
        "    {",
        "        return false;",
        "    }",
        "}",
        f"inline bool tbx_register_serializable_type(const {type_info.name}*)",
        "{",
        f"    return ::tbx::internal::register_serializable_type<{type_info.name}>(",
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
        "TBX_INTERNAL_AUTO_REGISTER(",
        "    tbx_serializable_type_registration_,",
        f"    tbx_register_serializable_type(static_cast<const {type_info.name}*>(nullptr)));",
        "",
    ]


def emit_indexed(type_info: SerializableType, count: str) -> list[str]:
    return [
        "template <typename BasicJsonType>",
        f"void to_json(BasicJsonType& tbx_json, const {type_info.name}& tbx_value)",
        "{",
        "    tbx_json = ::tbx::internal::write_indexed_serialization_value<BasicJsonType>(",
        "        tbx_value,",
        f"        {count});",
        "}",
        "template <typename BasicJsonType>",
        f"void from_json(const BasicJsonType& tbx_json, {type_info.name}& tbx_value)",
        "{",
        "    ::tbx::internal::read_indexed_serialization_value(",
        "        tbx_json,",
        "        tbx_value,",
        f"        {count});",
        "}",
        "",
    ] + emit_serializable_registration(type_info)
