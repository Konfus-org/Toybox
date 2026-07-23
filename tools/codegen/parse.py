"""libclang AST walk: turn annotated C++ headers into the model IR.

Annotations reach us as `clang::annotate` strings on child ANNOTATE_ATTR cursors (raw `[[tbx::...]]`
attributes are dropped by libclang, which is why attributes.h expands the macros to clang::annotate).
"""

from __future__ import annotations

import os
import re

import clang.cindex as cx

from model import (
    CodegenError,
    EnumDef,
    Field,
    FreeFunction,
    Method,
    Module,
    SerializerSpec,
    TypeDef,
)

_SERIALIZABLE_RE = re.compile(r"^tbx::serializable\((.*)\)$", re.DOTALL)
_DEFAULT_CLANG_ARGS = ["-x", "c++", "-std=c++23", "-DTBX_CODEGEN", "-ferror-limit=0"]
_VALID_FORMATS = ("DEFAULT", "TEXT", "CUSTOM")
# We only need declaration shapes: skipping function bodies and tolerating an incomplete TU keeps parse
# time low even when a header transitively pulls in heavy third-party templates (entt, glm) that are not
# on the include path — the annotated structs and their field names resolve regardless.
_PARSE_OPTIONS = (
    cx.TranslationUnit.PARSE_SKIP_FUNCTION_BODIES | cx.TranslationUnit.PARSE_INCOMPLETE
)


def default_clang_args(extra: list[str] | None = None) -> list[str]:
    return [*_DEFAULT_CLANG_ARGS, *(extra or [])]


def _annotations(cursor) -> list[str]:
    # libclang's Python bindings raise ValueError enumerating some cursor/template-arg kinds they don't
    # know (older bundled libclang vs C++23); skip any child that trips it — our annotated declarations
    # are ordinary top-level structs/enums/functions, never the deep template nodes that fail.
    found: list[str] = []
    try:
        children = list(cursor.get_children())
    except ValueError:
        return found
    for child in children:
        try:
            if child.kind == cx.CursorKind.ANNOTATE_ATTR:
                found.append(child.spelling)
        except ValueError:
            continue
    return found


# C++ field type -> the type it reads as from Luau (for the generated .d.luau). Anything unmapped is
# "any"; the .d.luau emitter skips "any" fields (runtime-only bags like UI::bindings drop out cleanly).
_LUAU_SCALARS = {
    "bool": "boolean",
    "float": "number",
    "double": "number",
    "int": "number",
    "int8": "number",
    "int16": "number",
    "int32": "number",
    "int64": "number",
    "uint8": "number",
    "uint16": "number",
    "uint32": "number",
    "uint64": "number",
    "std::string": "string",
    "Uuid": "string",
    "Vec2": "Vec2",
    "Vec3": "Vec3",
    "Vec4": "Vec4",
    "Quat": "Quat",
    "Color": "Color",
}


def _luau_type(clang_type) -> str:
    # Spelling-first: the written type name is stable, whereas canonical-kind introspection is flaky in
    # a big amalgam TU (and libclang's bindings raise ValueError on some C++23 template-arg kinds). An
    # unmappable type falls back to "any", which the .d.luau emitter drops (runtime-only bags vanish).
    try:
        spelling = clang_type.spelling.replace("tbx::", "").replace("std::", "")
    except ValueError:
        return "any"
    if spelling.startswith("AssetHandle<"):
        return "string"  # asset handles author as a path/uuid string
    if spelling.startswith("vector<AssetHandle<"):
        return "{string}"
    if spelling in _LUAU_SCALARS:
        return _LUAU_SCALARS[spelling]
    try:
        if clang_type.get_declaration().kind == cx.CursorKind.ENUM_DECL:
            return "number"  # enums serialize/read as their integer value
    except ValueError:
        pass
    return "any"


def _header_include(path: str) -> str:
    """The `tbx/...` include path for a source file (falls back to its basename)."""
    posix = path.replace("\\", "/")
    marker = "/tbx/"
    at = posix.rfind(marker)
    if at != -1:
        return "tbx/" + posix[at + len(marker) :]
    return os.path.basename(path)


def _split_top_level(text: str) -> list[str]:
    """Split on commas that are not inside quotes or (), <> — annotation args are simple, so this holds."""
    parts: list[str] = []
    depth = 0
    quote = None
    current = ""
    for ch in text:
        if quote is not None:
            current += ch
            if ch == quote:
                quote = None
            continue
        if ch in "\"'":
            quote = ch
            current += ch
            continue
        if ch in "(<":
            depth += 1
        elif ch in ")>":
            depth -= 1
        if ch == "," and depth == 0:
            parts.append(current.strip())
            current = ""
        else:
            current += ch
    if current.strip():
        parts.append(current.strip())
    return parts


def _parse_serializable(payload: str, type_name: str) -> tuple[SerializerSpec | None, str | None]:
    """Parse a `tbx::serializable(...)` annotation → (serializer or None, wire-name override or None)."""
    match = _SERIALIZABLE_RE.match(payload)
    inner = match.group(1).strip() if match else ""
    fmt = reader = writer = name = None
    for arg in _split_top_level(inner):
        if not arg:
            continue
        if arg.startswith("SerializerFormat::"):
            fmt = arg.split("::", 1)[1].strip()
        elif "=" in arg:
            key, value = (part.strip() for part in arg.split("=", 1))
            if key == "reader":
                reader = value.lstrip("&").strip()
            elif key == "writer":
                writer = value.lstrip("&").strip()
            elif key == "name":
                name = value.strip().strip('"')
            elif key == "version":
                pass  # reserved; migration hooks are not driven from attributes yet
            else:
                raise CodegenError(f"{type_name}: unknown TBX_SERIALIZABLE argument '{key}'")
        else:
            raise CodegenError(f"{type_name}: unexpected TBX_SERIALIZABLE argument '{arg}'")

    serializer = None
    if fmt is not None:
        if fmt not in _VALID_FORMATS:
            raise CodegenError(f"{type_name}: unknown SerializerFormat::{fmt}")
        if fmt == "CUSTOM" and not (reader or writer):
            raise CodegenError(
                f"{type_name}: SerializerFormat::CUSTOM needs reader= and/or writer="
            )
        if fmt in ("DEFAULT", "TEXT") and (reader or writer):
            raise CodegenError(
                f"{type_name}: SerializerFormat::{fmt} must not carry reader=/writer="
            )
        serializer = SerializerSpec(format=fmt, reader=reader, writer=writer)
    elif reader or writer:
        raise CodegenError(f"{type_name}: reader=/writer= requires a SerializerFormat")
    return serializer, name


def _build_type(cursor) -> TypeDef:
    annotations = _annotations(cursor)
    payload = next((a for a in annotations if a.startswith("tbx::serializable")), "")
    exposed = "tbx::exposed_to_scripting" in annotations
    serializer, name_override = _parse_serializable(payload, cursor.spelling)

    bases: list[str] = []
    fields: list[Field] = []
    methods: list[Method] = []
    for child in cursor.get_children():
        if child.kind == cx.CursorKind.CXX_BASE_SPECIFIER:
            bases.append(child.type.spelling.split("::")[-1])
        elif (
            child.kind == cx.CursorKind.FIELD_DECL
            and child.access_specifier == cx.AccessSpecifier.PUBLIC
        ):
            field_annotations = _annotations(child)
            try:
                spelling = child.type.spelling
            except ValueError:
                spelling = ""
            fields.append(
                Field(
                    name=child.spelling,
                    type_spelling=spelling,
                    luau_type=_luau_type(child.type),
                    is_serialized="tbx::do_not_serialize" not in field_annotations,
                    is_exposed_to_scripting=True,
                )
            )
        elif (
            child.kind == cx.CursorKind.CXX_METHOD
            and child.access_specifier == cx.AccessSpecifier.PUBLIC
            and not child.is_static_method()
        ):
            # Type spellings can trip the libclang-bindings template-arg-kind bug; the method NAME is
            # always safe, and signature detail is not consumed yet, so degrade gracefully.
            try:
                return_type = child.result_type.spelling
                param_types = [argument.type.spelling for argument in child.get_arguments()]
            except ValueError:
                return_type, param_types = "", []
            methods.append(Method(name=child.spelling, return_type=return_type, param_types=param_types))

    return TypeDef(
        name=cursor.spelling,
        wire_name=name_override or cursor.spelling,
        header=_header_include(str(cursor.location.file)),
        bases=bases,
        fields=fields,
        methods=methods,
        exposed_to_scripting=exposed,
        serializer=serializer,
    )


def _build_enum(cursor) -> EnumDef:
    values = [(c.spelling, c.enum_value) for c in cursor.get_children() if c.kind == cx.CursorKind.ENUM_CONSTANT_DECL]
    return EnumDef(name=cursor.spelling, header=_header_include(str(cursor.location.file)), values=values)


def _build_function(cursor) -> FreeFunction:
    try:
        params = [(argument.type.spelling, argument.spelling) for argument in cursor.get_arguments()]
        return_type = cursor.result_type.spelling
    except ValueError:
        params, return_type = [], ""
    return FreeFunction(
        name=cursor.spelling,
        header=_header_include(str(cursor.location.file)),
        return_type=return_type,
        params=params,
    )


def _collect(translation_unit, wanted_paths: set[str]) -> Module:
    """Gather annotated declarations that live in one of wanted_paths (not other includes).

    Sorted by header then source order so the emitted registration is stable across runs regardless of
    the order libclang happened to visit the amalgam's includes.
    """
    module = Module()
    for cursor in translation_unit.cursor.walk_preorder():
        location = cursor.location.file
        if location is None:
            continue
        if os.path.normcase(os.path.abspath(str(location))) not in wanted_paths:
            continue
        annotations = _annotations(cursor)
        if not annotations:
            continue
        if (
            cursor.kind in (cx.CursorKind.STRUCT_DECL, cx.CursorKind.CLASS_DECL)
            and cursor.is_definition()
            and any(a.startswith("tbx::serializable") for a in annotations)
        ):
            module.types.append(_build_type(cursor))
        elif (
            cursor.kind == cx.CursorKind.ENUM_DECL
            and cursor.is_definition()
            and "tbx::exposed_to_scripting" in annotations
        ):
            module.enums.append(_build_enum(cursor))
        elif cursor.kind == cx.CursorKind.FUNCTION_DECL and "tbx::exposed_to_scripting" in annotations:
            module.functions.append(_build_function(cursor))
    module.types.sort(key=lambda type_def: type_def.header)  # stable → source order kept within a header
    module.enums.sort(key=lambda enum_def: enum_def.header)
    module.functions.sort(key=lambda function: function.header)
    return module


def _normalized(paths) -> set[str]:
    return {os.path.normcase(os.path.abspath(path)) for path in paths}


def parse_amalgam(headers: list[str], clang_args: list[str]) -> Module:
    """Parse every annotated header in ONE translation unit.

    A single amalgam TU pays the cost of the heavy transitive includes (entt/glm/...) once instead of
    once per header — the difference between seconds and minutes across the whole engine.
    """
    amalgam_name = "__tbx_codegen_amalgam.cpp"
    body = "".join(f'#include "{_header_include(header)}"\n' for header in headers)
    translation_unit = cx.Index.create().parse(
        amalgam_name, args=clang_args, unsaved_files=[(amalgam_name, body)], options=_PARSE_OPTIONS
    )
    return _collect(translation_unit, _normalized(headers))


def parse_header(path: str, clang_args: list[str]) -> Module:
    translation_unit = cx.Index.create().parse(path, args=clang_args, options=_PARSE_OPTIONS)
    return _collect(translation_unit, _normalized([path]))


def parse_string(source: str, clang_args: list[str] | None = None, name: str = "test.h") -> Module:
    """Parse in-memory source — the test entry point."""
    args = clang_args if clang_args is not None else default_clang_args()
    translation_unit = cx.Index.create().parse(
        name, args=args, unsaved_files=[(name, source)], options=_PARSE_OPTIONS
    )
    return _collect(translation_unit, _normalized([name]))
