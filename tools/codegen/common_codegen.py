"""Shared type-name / version glue — rendered through the C++ template pack (``templates/cpp``)."""

from __future__ import annotations

from model import SerializableType, cpp_string, type_name, type_version
from render import render_lines


def emit_type_name(type_info: SerializableType) -> list[str]:
    return render_lines(
        "cpp/type_name.jinja",
        name=type_info.name,
        type_name=cpp_string(type_name(type_info)),
    )


def emit_version(type_info: SerializableType) -> list[str]:
    version = type_version(type_info)
    if version is None:
        return []
    return render_lines("cpp/version.jinja", name=type_info.name, version=version)
