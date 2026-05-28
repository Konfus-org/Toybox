from __future__ import annotations

import re

from model import Attribute, CodegenError, EnumValue, Field, SerializableType, attr_value, has_attr


ATTRIBUTE_PATTERN = re.compile(r"\[\[\s*tbx::([A-Za-z_]\w*)\s*(?:\((.*?)\))?\s*\]\]")
NAMESPACE_PATTERN = re.compile(r"^\s*namespace\s+([A-Za-z_]\w*(?:::[A-Za-z_]\w*)*)\s*(?:\{)?\s*$")
TYPE_PATTERN = re.compile(
    r"^\s*(struct|class)\s+(?:(?:[A-Za-z_]\w*_API|TBX_API)\s+)?([A-Za-z_]\w*)"
    r"(?:\s+final)?\s*(?::\s*([^{]+))?\s*(?:\{)?\s*$"
)
ENUM_PATTERN = re.compile(
    r"^\s*enum\s+(?:class\s+)?([A-Za-z_]\w*)\s*(?::\s*[A-Za-z_]\w*)?\s*(?:\{)?\s*$"
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


def parse_arguments(raw: str | None) -> list[str]:
    if raw is None or not raw.strip():
        return []

    args: list[str] = []
    current: list[str] = []
    in_string = False
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
            continue
        if character == "," and not in_string:
            args.append("".join(current).strip())
            current = []
            continue
        current.append(character)

    args.append("".join(current).strip())
    return args


def parse_attributes(text: str) -> list[Attribute]:
    return [
        Attribute(name=match.group(1), args=parse_arguments(match.group(2)))
        for match in ATTRIBUTE_PATTERN.finditer(text)
    ]


def remove_attributes(text: str) -> str:
    return ATTRIBUTE_PATTERN.sub("", text)


def collapse_multiline_attributes(source: str) -> str:
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
        kind = ""
        if has_attr(active_attrs, "prop"):
            kind = "prop"
        elif has_attr(active_attrs, "meta"):
            kind = "meta"
        elif has_attr(active_attrs, "text"):
            kind = "text"

        if not kind:
            continue

        match = FIELD_PATTERN.match(without_attrs)
        if not match:
            raise CodegenError(f"Could not parse attributed field declaration: {line.strip()}")

        fields.append(Field(name=match.group(2), kind=kind, json_name=attr_value(active_attrs, "name")))

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


def parse_source(source: str, source_path: str = "<memory>") -> list[SerializableType]:
    normalized_source = collapse_multiline_using_declarations(collapse_multiline_attributes(source))
    lines = normalized_source.splitlines()
    serializer_types = {match.group(1) for match in SERIALIZER_PATTERN.finditer(source)}
    pending_attrs: list[Attribute] = []
    serializable_types: list[SerializableType] = []

    index = 0
    while index < len(lines):
        line = lines[index]
        attrs = parse_attributes(line)
        without_attrs = remove_attributes(line).strip()

        if attrs and without_attrs == ";":
            pending_attrs.extend(attrs)
            index += 1
            continue
        if pending_attrs and not attrs and (not without_attrs or without_attrs.startswith("//")):
            index += 1
            continue

        active_attrs = pending_attrs + attrs
        pending_attrs = []
        is_serializable = has_attr(active_attrs, "serializable")

        type_match = TYPE_PATTERN.match(without_attrs)
        if type_match:
            end = find_matching_type_end(lines, index)
            if (
                is_serializable
                or has_attr(active_attrs, "printable")
                or has_attr(active_attrs, "hash")
                or has_attr(active_attrs, "plugin")
            ):
                type_name = type_match.group(2)
                serializable_types.append(
                    SerializableType(
                        namespace=current_namespace(lines, index),
                        name=type_name,
                        declaration_kind=type_match.group(1),
                        attrs=active_attrs,
                        bases=type_match.group(3) or "",
                        fields=parse_fields(lines, index, end),
                        has_serializer=type_name in serializer_types,
                        source_path=source_path,
                        line=index + 1,
                    )
                )
            index = end + 1
            continue

        enum_match = ENUM_PATTERN.match(without_attrs)
        if enum_match:
            end = find_matching_type_end(lines, index)
            if is_serializable or has_attr(active_attrs, "printable") or has_attr(active_attrs, "hash"):
                serializable_types.append(
                    SerializableType(
                        namespace=current_namespace(lines, index),
                        name=enum_match.group(1),
                        declaration_kind="enum",
                        attrs=active_attrs,
                        enum_values=parse_enum_values(lines, index, end),
                        source_path=source_path,
                        line=index + 1,
                    )
                )
            index = end + 1
            continue

        using_match = USING_PATTERN.match(without_attrs)
        if using_match and is_serializable:
            serializable_types.append(
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

    return serializable_types
