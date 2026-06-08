from __future__ import annotations

from model import SerializableType


def emit_variant_declarations(type_info: SerializableType) -> list[str]:
    return [
        f"void serialize(::tbx::Json& json, const {type_info.name}& value);",
        f"void deserialize(const ::tbx::Json& json, {type_info.name}& value);",
        "",
    ]


def emit_variant(type_info: SerializableType) -> list[str]:
    return [
        f"void serialize(::tbx::Json& json, const {type_info.name}& value)",
        "{",
        "    ::tbx::serialize_serializable_variant(json, value);",
        "}",
        f"void deserialize(const ::tbx::Json& json, {type_info.name}& value)",
        "{",
        "    ::tbx::deserialize_serializable_variant(json, value);",
        "}",
        "",
    ]
