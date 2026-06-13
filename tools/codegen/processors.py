"""Processor registry for Toybox metadata-driven code generation."""

from __future__ import annotations

from dataclasses import dataclass
from typing import Callable

from formatter_codegen import emit_formatter, emit_formatter_declaration
from hash_codegen import (
    emit_hash,
    emit_hash_declaration,
    emit_hash_equality,
    emit_hash_equality_declaration,
)
from model import Attribute, CodegenError, SerializableType, attr_spelling, has_attr


@dataclass
class CodegenContext:
    """Per-run state that processors can use while emitting fragments."""

    target: str


@dataclass
class AttributeSchema:
    """Shape-level validation for passive attributes."""

    max_positional_args: int | None = None
    named_args: frozenset[str] = frozenset()


class AttributeSchemaRegistry:
    """Validates generic attribute spelling before subsystem processors run."""

    def __init__(self) -> None:
        self._schemas = {
            "array": AttributeSchema(1, frozenset({"value"})),
            "app": AttributeSchema(None, frozenset({"name", "version"})),
            "category": AttributeSchema(1, frozenset({"value"})),
            "custom_serialization": AttributeSchema(2, frozenset({"read", "write"})),
            "description": AttributeSchema(1, frozenset({"value"})),
            "hidden": AttributeSchema(0, frozenset()),
            "inject": AttributeSchema(),
            "meta": AttributeSchema(None, frozenset({"fields"})),
            "name": AttributeSchema(1, frozenset({"value"})),
            "plugin": AttributeSchema(
                None,
                frozenset({"category", "dependencies", "name", "priority", "version"}),
            ),
            "post_deserialize": AttributeSchema(1, frozenset({"method"})),
            "post_serialize": AttributeSchema(1, frozenset({"method"})),
            "pre_deserialize": AttributeSchema(1, frozenset({"method"})),
            "pre_serialize": AttributeSchema(1, frozenset({"method"})),
            "printable": AttributeSchema(None, frozenset({"fields", "format"})),
            "prop": AttributeSchema(None, frozenset({"fields"})),
            "readonly": AttributeSchema(0, frozenset()),
            "register": AttributeSchema(2, frozenset({"factory", "service"})),
            "serializable": AttributeSchema(1, frozenset({"mode"})),
            "text": AttributeSchema(None, frozenset({"field"})),
            "version": AttributeSchema(1, frozenset({"value"})),
            "view": AttributeSchema(1, frozenset({"value"})),
        }

    def validate_attribute(self, owner_name: str, attr: Attribute) -> None:
        schema = self._schemas.get(attr.name)
        if schema is None:
            return
        spelling = attr_spelling(attr.name)
        if schema.max_positional_args is not None and len(attr.args) > schema.max_positional_args:
            raise CodegenError(
                f"{owner_name} uses [[{spelling}]] with too many positional arguments."
            )
        unsupported = sorted(set(attr.named_args) - set(schema.named_args))
        if unsupported:
            names = ", ".join(unsupported)
            raise CodegenError(
                f"{owner_name} uses [[{spelling}]] with unsupported named arguments: {names}."
            )

    def validate_type(self, type_info: SerializableType) -> None:
        for attr in type_info.attrs:
            self.validate_attribute(type_info.name, attr)
        for field in type_info.fields:
            for attr in field.attrs:
                self.validate_attribute(f"{type_info.name}.{field.name}", attr)


class CodegenProcessor:
    """Base class for one independent consumer of parsed metadata."""

    def interested(self, type_info: SerializableType) -> bool:
        return False

    def validate(self, context: CodegenContext, type_info: SerializableType) -> None:
        pass

    def emit_header(self, context: CodegenContext, type_info: SerializableType) -> list[str]:
        return []

    def emit_source(self, context: CodegenContext, type_info: SerializableType) -> list[str]:
        return []

    def global_header(self, type_info: SerializableType) -> list[str]:
        return []

    def global_source(self, type_info: SerializableType) -> list[str]:
        return []

    def header_includes(self, types: list[SerializableType]) -> list[str]:
        return []

    def source_includes(self, types: list[SerializableType]) -> list[str]:
        return []


class ServiceBindingProcessor(CodegenProcessor):
    """Emits runtime service registration and injection glue."""

    def __init__(
        self,
        has_runtime_service_glue: Callable[[SerializableType], bool],
        emit_runtime_service_declarations: Callable[[SerializableType], list[str]],
        emit_runtime_service_definitions: Callable[[SerializableType], list[str]],
    ) -> None:
        self._has_runtime_service_glue = has_runtime_service_glue
        self._emit_runtime_service_declarations = emit_runtime_service_declarations
        self._emit_runtime_service_definitions = emit_runtime_service_definitions

    def interested(self, type_info: SerializableType) -> bool:
        return self._has_runtime_service_glue(type_info)

    def emit_header(self, context: CodegenContext, type_info: SerializableType) -> list[str]:
        return self._emit_runtime_service_declarations(type_info)

    def emit_source(self, context: CodegenContext, type_info: SerializableType) -> list[str]:
        return self._emit_runtime_service_definitions(type_info)

    def header_includes(self, types: list[SerializableType]) -> list[str]:
        if any(self.interested(type_info) for type_info in types):
            return ['#include "tbx/systems/scripting/service_ref.h"']
        return []


class SerializationProcessor(CodegenProcessor):
    """Emits struct, enum, alias, asset, and script serialization glue."""

    def __init__(self, emit_serialization_type: Callable[[SerializableType, str], list[str]]) -> None:
        self._emit_serialization_type = emit_serialization_type

    def interested(self, type_info: SerializableType) -> bool:
        return has_attr(type_info.attrs, "serializable") or has_attr(type_info.attrs, "script")

    def emit_header(self, context: CodegenContext, type_info: SerializableType) -> list[str]:
        return self._emit_serialization_type(type_info, "header")

    def emit_source(self, context: CodegenContext, type_info: SerializableType) -> list[str]:
        return self._emit_serialization_type(type_info, "source")


class FormatterProcessor(CodegenProcessor):
    """Emits std::formatter specializations from printable metadata."""

    def interested(self, type_info: SerializableType) -> bool:
        return has_attr(type_info.attrs, "printable")

    def global_header(self, type_info: SerializableType) -> list[str]:
        return emit_formatter_declaration(type_info)

    def global_source(self, type_info: SerializableType) -> list[str]:
        return emit_formatter(type_info)


class HashProcessor(CodegenProcessor):
    """Emits std::hash specializations and generated equality operators."""

    def interested(self, type_info: SerializableType) -> bool:
        return has_attr(type_info.attrs, "hash")

    def emit_header(self, context: CodegenContext, type_info: SerializableType) -> list[str]:
        return emit_hash_equality_declaration(type_info)

    def emit_source(self, context: CodegenContext, type_info: SerializableType) -> list[str]:
        return emit_hash_equality(type_info)

    def global_header(self, type_info: SerializableType) -> list[str]:
        return emit_hash_declaration(type_info)

    def global_source(self, type_info: SerializableType) -> list[str]:
        return emit_hash(type_info)

    def source_includes(self, types: list[SerializableType]) -> list[str]:
        if any(self.interested(type_info) for type_info in types):
            return ['#include "tbx/utils/hash.h"']
        return []


class PluginProcessor(CodegenProcessor):
    """Marks plugin types so plugin source generation can take over."""

    def interested(self, type_info: SerializableType) -> bool:
        return has_attr(type_info.attrs, "plugin")


class AppProcessor(CodegenProcessor):
    """Marks app types so app entry-point generation can take over."""

    def interested(self, type_info: SerializableType) -> bool:
        return has_attr(type_info.attrs, "app")


class CodegenRegistry:
    """Runs processors in deterministic order over neutral metadata."""

    def __init__(self, processors: list[CodegenProcessor]):
        self._processors = processors
        self._attribute_schemas = AttributeSchemaRegistry()

    def active_types(self, types: list[SerializableType]) -> list[SerializableType]:
        return [type_info for type_info in types if self.interested(type_info)]

    def collect_global(self, method_name: str, type_info: SerializableType) -> list[str]:
        lines: list[str] = []
        for processor in self._processors:
            if processor.interested(type_info):
                lines.extend(getattr(processor, method_name)(type_info))
        return lines

    def emit(self, method_name: str, context: CodegenContext, type_info: SerializableType) -> list[str]:
        lines: list[str] = []
        for processor in self._processors:
            if processor.interested(type_info):
                lines.extend(getattr(processor, method_name)(context, type_info))
        return lines

    def emit_header(self, context: CodegenContext, type_info: SerializableType) -> list[str]:
        return self.emit("emit_header", context, type_info)

    def emit_source(self, context: CodegenContext, type_info: SerializableType) -> list[str]:
        return self.emit("emit_source", context, type_info)

    def global_header(self, type_info: SerializableType) -> list[str]:
        return self.collect_global("global_header", type_info)

    def global_source(self, type_info: SerializableType) -> list[str]:
        return self.collect_global("global_source", type_info)

    def header_includes(self, types: list[SerializableType]) -> list[str]:
        return sorted(
            {
                include
                for processor in self._processors
                for include in processor.header_includes(types)
            }
        )

    def interested(self, type_info: SerializableType) -> bool:
        return any(processor.interested(type_info) for processor in self._processors)

    def source_includes(self, types: list[SerializableType]) -> list[str]:
        return sorted(
            {
                include
                for processor in self._processors
                for include in processor.source_includes(types)
            }
        )

    def validate(self, types: list[SerializableType]) -> None:
        context = CodegenContext(target="validate")
        for type_info in types:
            self._attribute_schemas.validate_type(type_info)
            for processor in self._processors:
                if processor.interested(type_info):
                    processor.validate(context, type_info)
