from __future__ import annotations

from model import Field, SerializableType, cpp_string, json_key, sanitized_name
from struct_codegen import emit_json_functions


def emit_asset_type_registration(type_info: SerializableType, version: str) -> list[str]:
    return [
        f"inline std::true_type tbx_has_asset_serialization(const {type_info.name}*)",
        "{",
        "    return {};",
        "}",
        "TBX_INTERNAL_AUTO_REGISTER(",
        "    tbx_asset_type_registration_,",
        f"    ::tbx::internal::register_asset_type<{type_info.name}>({version}));",
        "",
    ]


def emit_asset_body(type_info: SerializableType, version: str, fields: list[Field]) -> list[str]:
    return (
        [
            f"inline std::true_type tbx_has_asset_json_fields(const {type_info.name}*)",
            "{",
            "    return {};",
            "}",
        ]
        + emit_json_functions(type_info, fields)
        + [
            "TBX_INTERNAL_AUTO_REGISTER(",
            "    tbx_asset_body_registration_,",
            f"    ::tbx::internal::register_asset_body_type<{type_info.name}>(",
            f"        {version},",
            f"        ::tbx::internal::read_json_asset_body<{type_info.name}>,",
            f"        ::tbx::internal::write_json_asset_body<{type_info.name}>));",
            "",
        ]
    )


def emit_asset_meta(type_info: SerializableType, version: str, fields: list[Field]) -> list[str]:
    helper = f"tbx_read_json_asset_meta_{sanitized_name(type_info.name)}"
    lines = [
        f"inline std::true_type tbx_has_meta_serialization(const {type_info.name}*)",
        "{",
        "    return {};",
        "}",
        f"inline std::true_type tbx_has_meta_json_fields(const {type_info.name}*)",
        "{",
        "    return {};",
        "}",
        f"inline ::tbx::Result {helper}(std::string_view data, {type_info.name}& asset)",
        "{",
        "    try",
        "    {",
        "        const auto json = ::tbx::JsonParser::parse(data);",
        f"        const {type_info.name} default_asset {{}};",
    ]
    for field in fields:
        lines.extend(
            [
                "        ::tbx::internal::read_serialization_field(",
                "            json,",
                f"            {cpp_string(json_key(field))},",
                f"            asset.{field.name},",
                f"            default_asset.{field.name});",
            ]
        )
    lines.extend(
        [
            "        return {};",
            "    }",
            "    catch (...)",
            "    {",
            "        return ::tbx::internal::make_serialization_failure(",
            "            \"Failed to parse Toybox asset meta JSON.\");",
            "    }",
            "}",
            "TBX_INTERNAL_AUTO_REGISTER(",
            "    tbx_asset_meta_registration_,",
            f"    ::tbx::internal::register_asset_meta_type<{type_info.name}>(",
            f"        {version},",
            f"        {helper}));",
            "",
        ]
    )
    return lines


def emit_custom_asset(type_info: SerializableType, version: str) -> list[str]:
    return [
        f"inline std::true_type tbx_has_custom_asset_serialization(const {type_info.name}*)",
        "{",
        "    return {};",
        "}",
        "TBX_INTERNAL_AUTO_REGISTER(",
        "    tbx_asset_body_registration_,",
        f"    ::tbx::internal::register_asset_body_type<{type_info.name}>(",
        f"        {version},",
        f"        ::tbx::internal::read_custom_json_asset_body<{type_info.name}>,",
        f"        ::tbx::internal::write_custom_json_asset_body<{type_info.name}>));",
        "",
    ]


def emit_text_asset(type_info: SerializableType, version: str, field: Field) -> list[str]:
    return [
        f"inline std::true_type tbx_has_text_serialization(const {type_info.name}*)",
        "{",
        "    return {};",
        "}",
        f"inline void tbx_set_text_serialization({type_info.name}& value, std::string text)",
        "{",
        f"    value.{field.name} = std::move(text);",
        "}",
        "TBX_INTERNAL_AUTO_REGISTER(",
        "    tbx_asset_body_registration_,",
        f"    ::tbx::internal::register_asset_body_type<{type_info.name}>(",
        f"        {version},",
        f"        [](std::string_view data, {type_info.name}& value)",
        "        {",
        f"            return ::tbx::internal::read_text_asset_body(data, value.{field.name});",
        "        },",
        f"        [](const {type_info.name}& value, std::string& output)",
        "        {",
        f"            return ::tbx::internal::write_text_asset_body(value.{field.name}, output);",
        "        }));",
        "",
    ]
