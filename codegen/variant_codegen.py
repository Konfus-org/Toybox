from __future__ import annotations

from model import SerializableType


def emit_variant(type_info: SerializableType) -> list[str]:
    return [
        f"inline void to_json(::tbx::Json& json, const {type_info.name}& value)",
        "{",
        "    ::tbx::internal::to_json_serializable_variant(json, value);",
        "}",
        f"inline void from_json(const ::tbx::Json& json, {type_info.name}& value)",
        "{",
        "    ::tbx::internal::from_json_serializable_variant(json, value);",
        "}",
        "",
    ]
