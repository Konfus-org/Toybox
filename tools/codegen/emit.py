"""Render the model IR to generated C++ via Jinja templates.

The repetitive `register_type<T>(...).field(...)` chains are assembled here (precise indentation) and
handed to the template as ready-made blocks; the templates stay thin skeletons. Files are only
rewritten when their content changes, so an unrelated header edit never forces a recompile of the
generated translation unit.
"""

from __future__ import annotations

from pathlib import Path

from jinja2 import Environment, FileSystemLoader

from model import TypeDef

_TEMPLATE_DIR = Path(__file__).parent / "templates"
_INDENT = " " * 8


def _field_options(field) -> str:
    parts: list[str] = []
    if not field.is_serialized:
        parts.append(".is_serialized = false")
    if not field.is_exposed_to_scripting:
        parts.append(".is_exposed_to_scripting = false")
    return f", FieldOptions {{{', '.join(parts)}}}" if parts else ""


def _registered_fields(type_def: TypeDef) -> list:
    """The fields register_type should reflect.

    CUSTOM/TEXT assets are opaque: their reader (or whole-file text) IS the decode, so reflection never
    touches their bytes — they register as a bare shape with no fields, exactly like the hand-written
    registration did. Reflect-only types and DEFAULT-format types reflect every public field (minus
    [[tbx::do_not_serialize]], which stays reflected only when it needs to; here it is simply excluded
    from serialization by the FieldOptions flag).
    """
    if type_def.serializer and type_def.serializer.format in ("CUSTOM", "TEXT"):
        return []
    return type_def.fields


def _type_block(type_def: TypeDef) -> str:
    head = f'{_INDENT}register_type<{type_def.name}>("{type_def.wire_name}")'
    fields = _registered_fields(type_def)
    if not fields:
        return head + ";"
    lines = [head]
    for index, field in enumerate(fields):
        last = index == len(fields) - 1
        lines.append(
            f"{_INDENT}    .field(\"{field.name}\", &{type_def.name}::{field.name}"
            f"{_field_options(field)}){';' if last else ''}"
        )
    return "\n".join(lines)


def _serializer_block(type_def: TypeDef) -> str:
    serializer = type_def.serializer
    lines = [
        f"{_INDENT}register_serializer<{type_def.name}>()",
        f"{_INDENT}    .format(SerializerFormat::{serializer.format})",
    ]
    if serializer.reader:
        lines.append(f"{_INDENT}    .deserializer({serializer.reader})")
    if serializer.writer:
        lines.append(f"{_INDENT}    .serializer({serializer.writer})")
    return "\n".join(lines) + ";"


def _environment() -> Environment:
    return Environment(
        loader=FileSystemLoader(str(_TEMPLATE_DIR)),
        trim_blocks=True,
        lstrip_blocks=True,
        keep_trailing_newline=True,
    )


def render_reflection(types: list[TypeDef]) -> tuple[str, str]:
    """Return (header_text, source_text) for the aggregated reflection.generated.{h,cpp}."""
    headers: list[str] = []
    for type_def in types:
        if type_def.header not in headers:
            headers.append(type_def.header)

    context = {
        "headers": headers,
        "type_blocks": [_type_block(t) for t in types],
        "serializer_blocks": [_serializer_block(t) for t in types if t.serializer],
    }
    environment = _environment()
    header_text = environment.get_template("reflection_generated_h.jinja").render(**context)
    source_text = environment.get_template("reflection_generated_cpp.jinja").render(**context)
    return header_text, source_text


def write_reflection(types: list[TypeDef], out_dir: str) -> list[str]:
    # Always (re)write: these files are an add_custom_command OUTPUT, so their mtime must advance past
    # the headers that triggered the run — otherwise CMake would re-invoke codegen every build after a
    # marked-header edit that happened to produce identical output. The command only runs when an input
    # actually changed, so the cost is one regeneration + one recompile of the generated TU.
    header_text, source_text = render_reflection(types)
    tbx_dir = Path(out_dir) / "tbx"
    tbx_dir.mkdir(parents=True, exist_ok=True)
    header_path = tbx_dir / "reflection.generated.h"
    source_path = tbx_dir / "reflection.generated.cpp"
    header_path.write_text(header_text, encoding="utf-8")
    source_path.write_text(source_text, encoding="utf-8")
    return [str(header_path), str(source_path)]
