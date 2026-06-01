"""Small C++ metadata parser for Toybox code generation.

This parser is intentionally shallow: it recognizes declarations and attributes
well enough to build metadata, then leaves all behavior decisions to processors.
"""

from __future__ import annotations

import re

from model import (
    Attribute,
    CodegenError,
    EnumValue,
    Field,
    SerializableType,
    attr_value,
    split_attribute_values,
)


ATTRIBUTE_PATTERN = re.compile(
    r"\[\[\s*(?:tbx::)?([A-Za-z_]\w*)\s*(?:\((.*?)\))?\s*\]\]"
)
NAMESPACE_PATTERN = re.compile(r"^\s*namespace\s+([A-Za-z_]\w*(?:::[A-Za-z_]\w*)*)\s*(?:\{)?\s*$")
TYPE_PATTERN = re.compile(
    r"^\s*(struct|class)\s+((?:[A-Za-z_]\w*_API|TBX_API)\s+)?([A-Za-z_]\w*)"
    r"(?:\s+final)?\s*(?::\s*([^{]+))?\s*(?:\{)?\s*$"
)
ENUM_PATTERN = re.compile(
    r"^\s*enum\s+(class\s+)?([A-Za-z_]\w*)\s*(?::\s*([A-Za-z_]\w*(?:::[A-Za-z_]\w*)?))?\s*(?:\{)?\s*$"
)
USING_PATTERN = re.compile(r"^\s*using\s+([A-Za-z_]\w*)\s*=\s*(.+?)\s*;")
SERIALIZER_PATTERN = re.compile(
    r"\bstruct\s+(?:(?:[A-Za-z_]\w*_API|TBX_API)\s+)?Serializer\s*<\s*([A-Za-z_]\w*)\s*>"
)
FIELD_PATTERN = re.compile(
    r"^\s*(?:(?:static|inline|constexpr|const)\s+)*(.+?)\s+([A-Za-z_]\w*)"
    r"(?:\s*(?:=|\{).*)?;\s*$"
)
ENUM_VALUE_PATTERN = re.compile(r"^\s*([A-Za-z_]\w*)\s*(.*?)(?:,|$)")
EQUALITY_OPERATOR_PATTERN = re.compile(r"\boperator\s*==")


def parse_arguments(raw: str | None, preserve_string_literals: bool = False) -> list[str]:
    if raw is None or not raw.strip():
        return []

    return split_attribute_values(raw, preserve_string_literals=preserve_string_literals)


def normalize_attribute_argument(argument: str) -> str:
    stripped = argument.strip()
    if len(stripped) >= 2 and stripped[0] == '"' and stripped[-1] == '"':
        return stripped[1:-1]
    return stripped


def split_named_argument(argument: str) -> tuple[str, str] | None:
    match = re.match(r"^\s*([A-Za-z_]\w*)\s*=\s*(.+?)\s*$", argument)
    if match is None:
        return None
    if match.group(2).lstrip().startswith("="):
        return None
    return match.group(1), match.group(2)


def parse_attribute_arguments(raw: str | None) -> tuple[list[str], dict[str, str]]:
    positional_args: list[str] = []
    named_args: dict[str, str] = {}
    for argument in parse_arguments(raw, preserve_string_literals=True):
        named_argument = split_named_argument(argument)
        if named_argument is None:
            positional_args.append(normalize_attribute_argument(argument))
            continue

        name, value = named_argument
        named_args[name] = normalize_attribute_argument(value)
    return positional_args, named_args


def parse_attributes(text: str) -> list[Attribute]:
    attrs: list[Attribute] = []
    for match in ATTRIBUTE_PATTERN.finditer(text):
        args, named_args = parse_attribute_arguments(match.group(2))
        attrs.append(Attribute(name=match.group(1), args=args, named_args=named_args))
    return attrs


def remove_attributes(text: str) -> str:
    return ATTRIBUTE_PATTERN.sub("", text)


def collapse_multiline_attributes(source: str) -> str:
    """Keep attribute blocks on one line before running the lightweight parser."""

    output: list[str] = []
    index = 0
    while index < len(source):
        if source.startswith("[[", index):
            end = source.find("]]", index + 2)
            if end != -1:
                attribute_text = source[index : end + 2].replace("\r", " ").replace("\n", " ")
                output.append(attribute_text)
                index = end + 2
                continue

        output.append(source[index])
        index += 1

    return "".join(output)


def collapse_multiline_using_declarations(source: str) -> str:
    lines = source.splitlines()
    output: list[str] = []
    pending: list[str] = []
    for line in lines:
        stripped = line.strip()
        if pending:
            pending.append(stripped)
            if ";" in stripped:
                output.append(" ".join(pending))
                pending = []
            continue

        if stripped.startswith("using ") and ";" not in stripped:
            pending.append(stripped)
            continue

        output.append(line)

    if pending:
        output.append(" ".join(pending))

    return "\n".join(output)


def find_matching_type_end(lines: list[str], start: int) -> int:
    depth = 0
    for index in range(start, len(lines)):
        depth += lines[index].count("{")
        depth -= lines[index].count("}")
        if depth <= 0 and "};" in lines[index]:
            return index
    raise CodegenError(f"Could not find end of type declaration starting at line {start + 1}.")


def current_namespace(lines: list[str], upto: int) -> str:
    namespace = ""
    for line in lines[: upto + 1]:
        match = NAMESPACE_PATTERN.match(line)
        if match:
            namespace = match.group(1)
    return namespace


def parse_fields(lines: list[str], start: int, end: int) -> list[Field]:
    """Parse every attributed field without assigning subsystem ownership."""

    fields: list[Field] = []
    pending: list[Attribute] = []

    for line in lines[start + 1 : end]:
        stripped = line.strip()
        if not stripped or stripped.startswith("//"):
            continue

        attrs = parse_attributes(line)
        without_attrs = remove_attributes(line).strip()
        if attrs and not without_attrs:
            pending.extend(attrs)
            continue

        active_attrs = pending + attrs
        pending = []
        if not active_attrs:
            continue

        match = FIELD_PATTERN.match(without_attrs)
        if not match:
            raise CodegenError(f"Could not parse attributed field declaration: {line.strip()}")

        fields.append(
            Field(
                name=match.group(2),
                type_name=match.group(1).strip(),
                attrs=active_attrs,
            )
        )

    return fields


def parse_enum_values(lines: list[str], start: int, end: int) -> list[EnumValue]:
    values: list[EnumValue] = []
    for line in lines[start + 1 : end]:
        stripped = line.strip()
        if not stripped or stripped.startswith("//"):
            continue

        match = ENUM_VALUE_PATTERN.match(stripped)
        if not match:
            continue

        name = match.group(1)
        if name in {"}", "{"}:
            continue

        attrs = parse_attributes(line)
        values.append(EnumValue(name=name, json_name=attr_value(attrs, "name") or name))

    return values


def has_equality_operator(lines: list[str], start: int, end: int) -> bool:
    return any(EQUALITY_OPERATOR_PATTERN.search(line) for line in lines[start + 1 : end])


def base_type_names(bases: str) -> list[str]:
    names: list[str] = []
    for base in bases.split(","):
        cleaned = re.sub(r"\b(public|private|protected|virtual)\b", "", base).strip()
        if not cleaned:
            continue
        names.append(cleaned.split()[-1].split("::")[-1])
    return names


def append_inherited_fields(types: list[SerializableType]) -> None:
    type_by_name = {type_info.name: type_info for type_info in types}

    def inherited_fields(type_info: SerializableType, visited: set[str]) -> list[Field]:
        fields: list[Field] = []
        for base_name in base_type_names(type_info.bases):
            if base_name in visited:
                continue
            base_type = type_by_name.get(base_name)
            if base_type is None:
                continue
            next_visited = visited | {base_name}
            fields.extend(inherited_fields(base_type, next_visited))
            fields.extend(base_type.fields)
        return fields

    for type_info in types:
        if not type_info.bases:
            continue
        inherited = inherited_fields(type_info, {type_info.name})
        if not inherited:
            continue

        existing = {field.name for field in type_info.fields}
        type_info.fields = [field for field in inherited if field.name not in existing] + type_info.fields


def parse_type_declarations(source: str, source_path: str) -> list[SerializableType]:
    """Return declarations carrying passive Toybox metadata.

    The parser deliberately stops at discovery. It does not decide whether
    serialization, hashing, plugins, or another subsystem owns an attribute.
    """

    normalized_source = collapse_multiline_using_declarations(collapse_multiline_attributes(source))
    lines = normalized_source.splitlines()
    serializer_types = {match.group(1) for match in SERIALIZER_PATTERN.finditer(source)}
    pending_attrs: list[Attribute] = []
    metadata_types: list[SerializableType] = []

    index = 0
    while index < len(lines):
        line = lines[index]
        attrs = parse_attributes(line)
        without_attrs = remove_attributes(line).strip()

        if attrs and without_attrs in {"", ";"}:
            pending_attrs.extend(attrs)
            index += 1
            continue
        if pending_attrs and not attrs and (not without_attrs or without_attrs.startswith("//")):
            index += 1
            continue

        active_attrs = pending_attrs + attrs
        pending_attrs = []

        type_match = TYPE_PATTERN.match(without_attrs)
        if type_match:
            end = find_matching_type_end(lines, index)
            type_name = type_match.group(3)
            fields = parse_fields(lines, index, end)
            if active_attrs or fields:
                metadata_types.append(
                    SerializableType(
                        namespace=current_namespace(lines, index),
                        name=type_name,
                        declaration_kind=type_match.group(1),
                        attrs=active_attrs,
                        api_macro=(type_match.group(2) or "").strip(),
                        bases=type_match.group(4) or "",
                        fields=fields,
                        has_serializer=type_name in serializer_types,
                        has_equality_operator=has_equality_operator(lines, index, end),
                        source_path=source_path,
                        line=index + 1,
                    )
                )
            index = end + 1
            continue

        enum_match = ENUM_PATTERN.match(without_attrs)
        if enum_match:
            end = find_matching_type_end(lines, index)
            if active_attrs:
                metadata_types.append(
                    SerializableType(
                        namespace=current_namespace(lines, index),
                        name=enum_match.group(2),
                        declaration_kind="enum",
                        attrs=active_attrs,
                        enum_values=parse_enum_values(lines, index, end),
                        enum_scoped=enum_match.group(1) is not None,
                        enum_underlying_type=enum_match.group(3) or "",
                        source_path=source_path,
                        line=index + 1,
                    )
                )
            index = end + 1
            continue

        using_match = USING_PATTERN.match(without_attrs)
        if using_match and active_attrs:
            metadata_types.append(
                SerializableType(
                    namespace=current_namespace(lines, index),
                    name=using_match.group(1),
                    declaration_kind="using",
                    attrs=active_attrs,
                    alias_value=using_match.group(2),
                    source_path=source_path,
                    line=index + 1,
                )
            )

        index += 1

    return metadata_types


def parse_source(
    source: str,
    source_path: str = "<memory>",
    context_source: str = "",
) -> list[SerializableType]:
    """Parse source plus include context into the neutral codegen IR."""

    context_types = parse_type_declarations(context_source, source_path) if context_source else []
    source_types = parse_type_declarations(source, source_path)
    all_types = context_types + source_types
    append_inherited_fields(all_types)
    return source_types
