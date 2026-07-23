"""Language-neutral IR the parser fills and the emitters render.

Kept deliberately small: Phase 1 (reflection + serialization registration) needs only field *names*
plus per-field options and a serializer spec — the generated `.field("x", &T::x)` lets the C++
`field_kind_of<>` deduce the kind at compile time, so the tool never has to model field kinds itself.
The richer members (type spellings, methods, enums, free functions) are captured for the Phase 2
scripting emitters.
"""

from __future__ import annotations

from dataclasses import dataclass, field


class CodegenError(Exception):
    """A malformed annotation the author must fix (e.g. a CUSTOM serializer with no reader/writer)."""


@dataclass
class Field:
    name: str
    type_spelling: str
    is_serialized: bool = True
    is_exposed_to_scripting: bool = True


@dataclass
class Method:
    name: str
    return_type: str
    param_types: list[str] = field(default_factory=list)


@dataclass
class SerializerSpec:
    format: str  # "DEFAULT" | "TEXT" | "CUSTOM"
    reader: str | None = None  # function identifier (the leading '&' stripped)
    writer: str | None = None


@dataclass
class TypeDef:
    name: str  # C++ class name, e.g. "Transform"
    wire_name: str  # name= override, else == name
    header: str  # include path, e.g. "tbx/math/transform.h"
    bases: list[str] = field(default_factory=list)  # ["Block"] / ["Asset"]
    fields: list[Field] = field(default_factory=list)  # public fields only
    methods: list[Method] = field(default_factory=list)
    exposed_to_scripting: bool = False
    serializer: SerializerSpec | None = None


@dataclass
class EnumDef:
    name: str
    header: str
    values: list[tuple[str, int]] = field(default_factory=list)


@dataclass
class FreeFunction:
    name: str
    header: str
    return_type: str
    params: list[tuple[str, str]] = field(default_factory=list)  # (type_spelling, name)


@dataclass
class Module:
    types: list[TypeDef] = field(default_factory=list)
    enums: list[EnumDef] = field(default_factory=list)
    functions: list[FreeFunction] = field(default_factory=list)
