"""Neutral metadata model shared by Toybox codegen processors."""

from __future__ import annotations

import dataclasses
import re


@dataclasses.dataclass
class Attribute:
    """A parsed C++ attribute with no behavior attached to it."""

    name: str
    args: list[str]
    named_args: dict[str, str] = dataclasses.field(default_factory=dict)


@dataclasses.dataclass
class Field:
    """Neutral field metadata discovered by the parser."""

    name: str
    type_name: str = ""
    attrs: list[Attribute] = dataclasses.field(default_factory=list)
    access: str = "public"


@dataclasses.dataclass
class EnumValue:
    """Enum value metadata, including the external name chosen by attributes."""

    name: str
    json_name: str


@dataclasses.dataclass
class SerializableType:
    """Neutral type metadata consumed by independent codegen processors."""

    namespace: str
    name: str
    declaration_kind: str
    attrs: list[Attribute]
    api_macro: str = ""
    bases: str = ""
    fields: list[Field] = dataclasses.field(default_factory=list)
    enum_values: list[EnumValue] = dataclasses.field(default_factory=list)
    enum_scoped: bool = False
    enum_underlying_type: str = ""
    alias_value: str = ""
    has_serializer: bool = False
    has_equality_operator: bool = False
    source_path: str = "<memory>"
    line: int = 0


class CodegenError(RuntimeError):
    pass


def split_attribute_values(raw: str, preserve_string_literals: bool = False) -> list[str]:
    if not raw.strip():
        return []

    values: list[str] = []
    current: list[str] = []
    angle_depth = 0
    brace_depth = 0
    in_string = False
    paren_depth = 0
    escape = False
    for character in raw:
        if escape:
            current.append(character)
            escape = False
            continue
        if character == "\\" and in_string:
            current.append(character)
            escape = True
            continue
        if character == '"':
            in_string = not in_string
            if preserve_string_literals:
                current.append(character)
            continue
        if not in_string and character == "(":
            paren_depth += 1
        elif not in_string and character == ")" and paren_depth > 0:
            paren_depth -= 1
        elif not in_string and character == "<":
            angle_depth += 1
        elif not in_string and character == ">" and angle_depth > 0:
            angle_depth -= 1
        elif not in_string and character == "{":
            brace_depth += 1
        elif not in_string and character == "}" and brace_depth > 0:
            brace_depth -= 1
        if (
            character == ","
            and not in_string
            and paren_depth == 0
            and angle_depth == 0
            and brace_depth == 0
        ):
            values.append("".join(current).strip())
            current = []
            continue
        current.append(character)

    values.append("".join(current).strip())
    return values


def attr_arg(
    attr: Attribute,
    index: int,
    name: str,
    default: str | None = None,
) -> str | None:
    value = attr.named_args.get(name)
    if value is None and len(attr.args) > index:
        value = attr.args[index]
    if value is None:
        return default

    value = value.strip()
    return value if value else default


def attr_list_arg(attr: Attribute, name: str) -> list[str]:
    value = attr.named_args.get(name)
    if value is None:
        return []

    value = value.strip()
    if value.startswith("{") and value.endswith("}"):
        value = value[1:-1]

    return [item for item in split_attribute_values(value) if item]


def attr_value(attrs: list[Attribute], name: str) -> str | None:
    for attr in attrs:
        if attr.name == name:
            return attr_arg(attr, 0, "value")
    return None


def attr_spelling(name: str) -> str:
    """Render an internal attribute name back as its fully-qualified C++ source spelling."""
    return f"tbx::{name}"


def attrs_named(attrs: list[Attribute], name: str) -> list[Attribute]:
    return [attr for attr in attrs if attr.name == name]


def find_attr(attrs: list[Attribute], name: str) -> Attribute | None:
    for attr in attrs:
        if attr.name == name:
            return attr
    return None


def has_attr(attrs: list[Attribute], name: str) -> bool:
    return find_attr(attrs, name) is not None


def cpp_string(value: str) -> str:
    return '"' + value.replace("\\", "\\\\").replace('"', '\\"') + '"'


def fields_of(type_info: SerializableType, kind: str) -> list[Field]:
    return [field for field in type_info.fields if has_attr(field.attrs, kind)]


# Field attributes that route a member through a dedicated channel rather than the default
# value-serialization path; such members are never auto-serialized as plain props.
SPECIAL_FIELD_ATTRS = ("meta", "text", "inject", "register")


def is_serialized_field(field: Field) -> bool:
    """Public members of a serializable type serialize by default; ``[[do_not_serialize]]`` opts a
    public member out and ``[[serialize]]`` opts a non-public member in. Members owned by a dedicated
    channel ([[meta]]/[[text]]/[[inject]]/[[register]]) are never treated as plain serialized props."""
    if has_attr(field.attrs, "do_not_serialize"):
        return False
    if any(has_attr(field.attrs, kind) for kind in SPECIAL_FIELD_ATTRS):
        return False
    if has_attr(field.attrs, "serialize"):
        return True
    return field.access == "public"


def serialized_fields(type_info: SerializableType) -> list[Field]:
    return [field for field in type_info.fields if is_serialized_field(field)]


def non_public_serialized_fields(type_info: SerializableType) -> list[Field]:
    return [field for field in serialized_fields(type_info) if field.access != "public"]


def is_asset(type_info: SerializableType) -> bool:
    return "Asset" in type_info.bases


def json_key(field: Field) -> str:
    return external_name(field)


def external_name(metadata: Field | SerializableType | EnumValue) -> str:
    if isinstance(metadata, Field):
        value = attr_value(metadata.attrs, "name")
        if value:
            return value
        return metadata.name[1:] if metadata.name.startswith("_") else metadata.name
    if isinstance(metadata, SerializableType):
        value = attr_value(metadata.attrs, "name")
        if value:
            return value
        return metadata.name
    return metadata.json_name


def qualified_name(type_info: SerializableType) -> str:
    if not type_info.namespace:
        return type_info.name
    return f"{type_info.namespace}::{type_info.name}"


def sanitized_name(name: str) -> str:
    return re.sub(r"[^A-Za-z0-9_]", "_", name)


def type_name(type_info: SerializableType) -> str:
    return external_name(type_info)


def type_version(type_info: SerializableType) -> str | None:
    return attr_value(type_info.attrs, "version")
