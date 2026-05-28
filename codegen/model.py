from __future__ import annotations

import dataclasses
import re


@dataclasses.dataclass
class Attribute:
    name: str
    args: list[str]


@dataclasses.dataclass
class Field:
    name: str
    kind: str
    json_name: str | None = None


@dataclasses.dataclass
class EnumValue:
    name: str
    json_name: str


@dataclasses.dataclass
class SerializableType:
    namespace: str
    name: str
    declaration_kind: str
    attrs: list[Attribute]
    bases: str = ""
    fields: list[Field] = dataclasses.field(default_factory=list)
    enum_values: list[EnumValue] = dataclasses.field(default_factory=list)
    alias_value: str = ""
    has_serializer: bool = False
    source_path: str = "<memory>"
    line: int = 0


class CodegenError(RuntimeError):
    pass


def attr_value(attrs: list[Attribute], name: str) -> str | None:
    for attr in attrs:
        if attr.name == name and attr.args:
            return attr.args[0]
    return None


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
    return [field for field in type_info.fields if field.kind == kind]


def is_asset(type_info: SerializableType) -> bool:
    return "Asset" in type_info.bases


def json_key(field: Field) -> str:
    if field.json_name:
        return field.json_name
    return field.name[1:] if field.name.startswith("_") else field.name


def qualified_name(type_info: SerializableType) -> str:
    if not type_info.namespace:
        return type_info.name
    return f"{type_info.namespace}::{type_info.name}"


def sanitized_name(name: str) -> str:
    return re.sub(r"[^A-Za-z0-9_]", "_", name)


def type_name(type_info: SerializableType) -> str:
    return attr_value(type_info.attrs, "name") or type_info.name


def type_version(type_info: SerializableType) -> str | None:
    return attr_value(type_info.attrs, "version")
