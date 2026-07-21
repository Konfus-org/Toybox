"""Enum serialization glue — rendered through the C++ template pack (``templates/cpp/enum_*``)."""

from __future__ import annotations

from model import SerializableType, cpp_string
from render import render_lines


def _enum_values(type_info: SerializableType) -> list[dict[str, str]]:
    """Pre-resolve each enumerator's C++ label and quoted JSON name for the template."""
    return [
        {
            "label": f"{type_info.name}::{value.name}" if type_info.enum_scoped else value.name,
            "json": cpp_string(value.json_name),
        }
        for value in type_info.enum_values
    ]


def emit_enum_declarations(type_info: SerializableType) -> list[str]:
    return render_lines("cpp/enum_declarations.jinja", name=type_info.name)


def emit_enum(type_info: SerializableType) -> list[str]:
    return render_lines(
        "cpp/enum_definition.jinja",
        name=type_info.name,
        values=_enum_values(type_info),
    )
