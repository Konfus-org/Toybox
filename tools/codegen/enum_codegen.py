from __future__ import annotations

from model import SerializableType, cpp_string


def emit_enum_declarations(type_info: SerializableType) -> list[str]:
    return [
        f"void serialize(::tbx::Json& json, const {type_info.name}& value);",
        f"void deserialize(const ::tbx::Json& json, {type_info.name}& value);",
        # ADL hook advertising the enum's members so attribute serialization can render a property of
        # this type as a dropdown (replaces the old enum reflection record's enum_values).
        f"std::vector<std::string> property_choices(const {type_info.name}*);",
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
            # Editor-authored files write enums as their underlying numeric values (a name-mangling
            # scheme between C# and C++ spellings can't round-trip every member); accept both.
            "    if (json.is_number_integer())",
            "    {",
            f"        value = static_cast<{type_info.name}>(json.get<int>());",
            "        return;",
            "    }",
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
    values = ", ".join(cpp_string(enum_value.json_name) for enum_value in type_info.enum_values)
    lines.extend(
        [
            f"std::vector<std::string> property_choices(const {type_info.name}*)",
            "{",
            f"    return {{ {values} }};",
            "}",
            "",
        ]
    )
    return lines
