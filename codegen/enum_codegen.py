from __future__ import annotations

from model import SerializableType, cpp_string


def emit_enum(type_info: SerializableType) -> list[str]:
    entries = [
        f"        {{{type_info.name}::{value.name}, {cpp_string(value.json_name)}}},"
        for value in type_info.enum_values
    ]
    return [
        f"NLOHMANN_JSON_SERIALIZE_ENUM({type_info.name},",
        "    {",
        f"        {{static_cast<{type_info.name}>(0), nullptr}},",
        *entries,
        "    })",
        "",
    ]
