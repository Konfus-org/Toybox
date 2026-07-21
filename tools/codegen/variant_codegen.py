"""std::variant serialization glue — rendered through the C++ template pack (``templates/cpp``)."""

from __future__ import annotations

from model import SerializableType
from render import render_lines


def emit_variant_declarations(type_info: SerializableType) -> list[str]:
    # Declarations are the plain serialize/deserialize pair, identical to the enum decl shape.
    return render_lines("cpp/enum_declarations.jinja", name=type_info.name)


def emit_variant(type_info: SerializableType) -> list[str]:
    return render_lines("cpp/variant_definition.jinja", name=type_info.name)
