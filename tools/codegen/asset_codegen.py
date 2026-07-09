from __future__ import annotations

from model import Field, SerializableType, cpp_string, json_key, sanitized_name
from struct_codegen import (
    attribute_descriptor_literal,
    emit_json_function_declarations,
    emit_json_function_definitions,
)


def emit_asset_type_registration_declarations(type_info: SerializableType) -> list[str]:
    return [
        f"std::true_type has_asset_serialization(const {type_info.name}*);",
        "",
    ]


def emit_asset_type_registration(type_info: SerializableType, version: str) -> list[str]:
    return [
        f"std::true_type has_asset_serialization(const {type_info.name}*)",
        "{",
        "    return {};",
        "}",
        "TBX_SERIALIZATION_AUTO_REGISTER(",
        "    tbx_asset_type_registration_,",
        f"    ::tbx::register_asset_type<{type_info.name}>({version}));",
        "",
    ]


def emit_asset_body_declarations(type_info: SerializableType) -> list[str]:
    return [
        f"std::true_type has_asset_json_fields(const {type_info.name}*);",
        *emit_json_function_declarations(type_info),
    ]


def emit_asset_body(type_info: SerializableType, version: str, fields: list[Field]) -> list[str]:
    return (
        [
            f"std::true_type has_asset_json_fields(const {type_info.name}*)",
            "{",
            "    return {};",
            "}",
        ]
        + emit_json_function_definitions(type_info, fields, force_keyed=True)
        + [
            "TBX_SERIALIZATION_AUTO_REGISTER(",
            "    tbx_asset_body_registration_,",
            f"    ::tbx::register_asset_body_type<{type_info.name}>(",
            f"        {version},",
            f"        ::tbx::read_json_asset_body<{type_info.name}>,",
            f"        ::tbx::write_json_asset_body<{type_info.name}>));",
            "",
        ]
    )


def emit_asset_meta_declarations(type_info: SerializableType) -> list[str]:
    read_helper = f"read_json_asset_meta_{sanitized_name(type_info.name)}"
    write_helper = f"write_json_asset_meta_{sanitized_name(type_info.name)}"
    source_path = type_info.source_path.replace("\\", "/")
    api_macro = (
        type_info.api_macro
        or ("TBX_API" if type_info.namespace == "tbx" and "/engine/include/" in source_path else "")
    )
    api_prefix = f"{api_macro} " if api_macro else ""
    return [
        f"std::true_type has_meta_serialization(const {type_info.name}*);",
        f"std::true_type has_meta_json_fields(const {type_info.name}*);",
        f"{api_prefix}::tbx::Result {read_helper}(std::string_view data, {type_info.name}& asset);",
        f"{api_prefix}::tbx::Result {write_helper}(const {type_info.name}& asset, std::string& output);",
        "",
    ]


def emit_asset_meta(type_info: SerializableType, version: str, fields: list[Field]) -> list[str]:
    read_helper = f"read_json_asset_meta_{sanitized_name(type_info.name)}"
    write_helper = f"write_json_asset_meta_{sanitized_name(type_info.name)}"
    lines = [
        f"std::true_type has_meta_serialization(const {type_info.name}*)",
        "{",
        "    return {};",
        "}",
        f"std::true_type has_meta_json_fields(const {type_info.name}*)",
        "{",
        "    return {};",
        "}",
        # Reader: tolerant of both the flat on-disk .meta (bare values) and the editor's typed save
        # (the { type, value } / { attributes, value } wrappers asset.describe round-trips), so the same
        # field decode serves loading from disk and applying an inspector edit.
        f"::tbx::Result {read_helper}(std::string_view data, {type_info.name}& asset)",
        "{",
        "    try",
        "    {",
        "        const auto json = ::tbx::JsonParser::parse(data);",
        f"        const {type_info.name} default_asset {{}};",
    ]
    for field in fields:
        lines.extend(
            [
                "        ::tbx::read_typed_serialization_field(",
                "            json,",
                f"            {cpp_string(json_key(field))},",
                f"            asset.{field.name},",
                f"            default_asset.{field.name});",
            ]
        )
    lines.extend(
        [
            "        return ::tbx::Result();",
            "    }",
            "    catch (...)",
            "    {",
            "        return ::tbx::make_serialization_failure(",
            "            \"Failed to parse Toybox asset meta JSON.\");",
            "    }",
            "}",
            # Writer: flat bare values for persistence (the .meta sidecar's historical shape), the
            # attribute-rich shape under the editor's AttributeSerializationScope so the inspector can
            # describe a meta-only asset (e.g. a texture's import settings) with enum dropdowns/categories.
            f"::tbx::Result {write_helper}(const {type_info.name}& asset, std::string& output)",
            "{",
            "    try",
            "    {",
            "        auto json = ::tbx::Json::object();",
            f"        const {type_info.name} default_asset {{}};",
        ]
    )
    for order, field in enumerate(fields):
        lines.extend(
            [
                "        ::tbx::write_meta_serialization_field(",
                "            json,",
                f"            {cpp_string(json_key(field))},",
                f"            asset.{field.name},",
                f"            default_asset.{field.name},",
                f"            {attribute_descriptor_literal(field, order)});",
            ]
        )
    lines.extend(
        [
            "        output = json.dump(4);",
            "        return ::tbx::Result();",
            "    }",
            "    catch (...)",
            "    {",
            "        return ::tbx::make_serialization_failure(",
            "            \"Failed to serialize Toybox asset meta JSON.\");",
            "    }",
            "}",
            "TBX_SERIALIZATION_AUTO_REGISTER(",
            "    tbx_asset_meta_registration_,",
            f"    ::tbx::register_asset_meta_type<{type_info.name}>(",
            f"        {version},",
            f"        {read_helper},",
            f"        {write_helper}));",
            "",
        ]
    )
    return lines


def emit_custom_asset_declarations(type_info: SerializableType) -> list[str]:
    return [
        f"std::true_type has_custom_asset_serialization(const {type_info.name}*);",
        "",
    ]


def emit_custom_asset(
    type_info: SerializableType,
    version: str,
    write_callable: str,
    read_callable: str,
) -> list[str]:
    return [
        f"std::true_type has_custom_asset_serialization(const {type_info.name}*)",
        "{",
        "    return {};",
        "}",
        "TBX_SERIALIZATION_AUTO_REGISTER(",
        "    tbx_asset_body_registration_,",
        f"    ::tbx::register_asset_body_type<{type_info.name}>(",
        f"        {version},",
        f"        [](std::string_view data, {type_info.name}& value)",
        "        {",
        "            ::tbx::pre_deserialize(value);",
        f"            if (!{read_callable}(data, value))",
        "                return ::tbx::make_serialization_failure(",
        "                    \"Failed to parse custom Toybox asset body.\");",
        "            ::tbx::post_deserialize(value);",
        "            return ::tbx::Result();",
        "        },",
        f"        [](const {type_info.name}& value, std::string& output)",
        "        {",
        "            ::tbx::pre_serialize(value);",
        f"            output = {write_callable}(value);",
        "            ::tbx::post_serialize(value);",
        "            return ::tbx::Result();",
        "        }));",
        "",
    ]


def emit_text_asset_declarations(type_info: SerializableType) -> list[str]:
    return [
        f"std::true_type has_text_serialization(const {type_info.name}*);",
        f"void set_text_serialization({type_info.name}& value, std::string text);",
        "",
    ]


def emit_text_asset(type_info: SerializableType, version: str, field: Field) -> list[str]:
    return [
        f"std::true_type has_text_serialization(const {type_info.name}*)",
        "{",
        "    return {};",
        "}",
        f"void set_text_serialization({type_info.name}& value, std::string text)",
        "{",
        f"    value.{field.name} = std::move(text);",
        "}",
        "TBX_SERIALIZATION_AUTO_REGISTER(",
        "    tbx_asset_body_registration_,",
        f"    ::tbx::register_asset_body_type<{type_info.name}>(",
        f"        {version},",
        f"        [](std::string_view data, {type_info.name}& value)",
        "        {",
        f"            return ::tbx::read_text_asset_body(data, value.{field.name});",
        "        },",
        f"        [](const {type_info.name}& value, std::string& output)",
        "        {",
        f"            return ::tbx::write_text_asset_body(value.{field.name}, output);",
        "        }));",
        "",
    ]
