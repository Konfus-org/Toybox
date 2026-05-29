from __future__ import annotations

from model import SerializableType, cpp_string


def emit_enum_declarations(type_info: SerializableType) -> list[str]:
    return [
        f"void serialize(::tbx::Json& json, const {type_info.name}& value);",
        f"void deserialize(const ::tbx::Json& json, {type_info.name}& value);",
        "",
    ]


def emit_enum(type_info: SerializableType) -> list[str]:
    lines = [
        f"void serialize(::tbx::Json& json, const {type_info.name}& value)",
        "{",
        "    switch (value)",
        "    {",
    ]
    for enum_value in type_info.enum_values:
        enum_value_name = (
            f"{type_info.name}::{enum_value.name}"
            if type_info.enum_scoped
            else enum_value.name
        )
        lines.extend(
            [
                f"        case {enum_value_name}:",
                f"            json = {cpp_string(enum_value.json_name)};",
                "            return;",
            ]
        )
    lines.extend(
        [
            "        default:",
            "            json = nullptr;",
            "            return;",
            "    }",
            "}",
            f"void deserialize(const ::tbx::Json& json, {type_info.name}& value)",
            "{",
            "    if (!json.is_string())",
            "    {",
            f"        value = static_cast<{type_info.name}>(0);",
            "        return;",
            "    }",
            "    const auto name = json.get<std::string>();",
        ]
    )
    for enum_value in type_info.enum_values:
        enum_value_name = (
            f"{type_info.name}::{enum_value.name}"
            if type_info.enum_scoped
            else enum_value.name
        )
        lines.extend(
            [
                f"    if (name == {cpp_string(enum_value.json_name)})",
                "    {",
                f"        value = {enum_value_name};",
                "        return;",
                "    }",
            ]
        )
    lines.extend(
        [
            f"    value = static_cast<{type_info.name}>(0);",
            "}",
            "",
        ]
    )
    return lines
