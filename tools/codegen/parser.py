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


# Toybox attributes live in the `tbx::` attribute namespace. Engine code (which is itself inside
# `namespace tbx`) omits the scope and writes the bare name, e.g. [[serializable]] / [[serialize]];
# examples and plugins write it out in full, e.g. [[tbx::serializable]] / [[tbx::serialize]]. Both
# forms normalize to the same bare captured name.
ATTRIBUTE_PATTERN = re.compile(
    r"\[\[\s*(?:tbx::)?([A-Za-z_]\w*)\s*(?:\((.*?)\))?\s*\]\]"
)
NAMESPACE_PATTERN = re.compile(r"^\s*namespace\s+([A-Za-z_]\w*(?:::[A-Za-z_]\w*)*)\s*(?:\{)?\s*$")
TYPE_PATTERN = re.compile(
    r"^\s*(struct|class)\s+((?:[A-Za-z_]\w*_API|TBX_API)\s+)?([A-Za-z_]\w*)"
    r"(?:\s+final)?\s*(?::\s*([^{]+))?\s*(?:\{)?\s*$"
)
TEMPLATE_PATTERN = re.compile(r"^\s*template\s*<(.+)>\s*$")
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


ACCESS_LABELS = {"public", "private", "protected"}
# Leading keywords that mark a class-body declaration as something other than a serializable data
# member (type aliases, friends, statics, function specifiers, nested types, templates).
_NON_MEMBER_LEADING = re.compile(
    r"^(?:using|typedef|friend|static|constexpr|consteval|constinit|inline|virtual|explicit|"
    r"template|struct|class|union|enum)\b"
)


def _skip_string(text: str, index: int) -> int:
    """``text[index]`` is a quote character; return the index just past the closing quote."""
    quote = text[index]
    index += 1
    while index < len(text):
        character = text[index]
        if character == "\\":
            index += 2
            continue
        if character == quote:
            return index + 1
        index += 1
    return index


def _skip_block(text: str, index: int) -> int:
    """``text[index]`` is ``{``; return the index just past the matching ``}``."""
    depth = 0
    while index < len(text):
        character = text[index]
        if character in "\"'":
            index = _skip_string(text, index)
            continue
        if text.startswith("//", index):
            newline = text.find("\n", index)
            index = len(text) if newline == -1 else newline
            continue
        if text.startswith("/*", index):
            close = text.find("*/", index + 2)
            index = len(text) if close == -1 else close + 2
            continue
        if text.startswith("[[", index):
            close = text.find("]]", index + 2)
            index = len(text) if close == -1 else close + 2
            continue
        if character == "{":
            depth += 1
        elif character == "}":
            depth -= 1
            if depth == 0:
                return index + 1
        index += 1
    return index


def _extract_class_body(type_text: str) -> str:
    """Return the text strictly inside a type's outermost ``{ ... }`` braces."""
    index = 0
    while index < len(type_text):
        character = type_text[index]
        if character in "\"'":
            index = _skip_string(type_text, index)
            continue
        if type_text.startswith("//", index):
            newline = type_text.find("\n", index)
            index = len(type_text) if newline == -1 else newline
            continue
        if type_text.startswith("/*", index):
            close = type_text.find("*/", index + 2)
            index = len(type_text) if close == -1 else close + 2
            continue
        if type_text.startswith("[[", index):
            close = type_text.find("]]", index + 2)
            index = len(type_text) if close == -1 else close + 2
            continue
        if character == "{":
            close = _skip_block(type_text, index)
            return type_text[index + 1 : close - 1]
        index += 1
    return ""


def _remove_angle_groups(text: str) -> str:
    """Strip balanced ``<...>`` template-argument groups so a parameter-list ``(`` can be detected
    without confusing it for a ``(`` nested inside a template argument (e.g. std::function<void()>)."""
    out: list[str] = []
    depth = 0
    for character in text:
        if character == "<":
            depth += 1
        elif character == ">":
            if depth > 0:
                depth -= 1
        elif depth == 0:
            out.append(character)
    return "".join(out)


def _make_field_from_head(head: str, access: str) -> Field | None:
    """Build a data-member Field from a class-body declaration head, or None when the declaration is
    not a serializable data member (function, ctor/dtor, operator, alias, nested type, static, ...)."""
    attrs = parse_attributes(head)
    declaration = re.sub(r"\s+", " ", remove_attributes(head)).strip()
    if not declaration:
        return None
    if _NON_MEMBER_LEADING.match(declaration):
        return None
    if re.search(r"\boperator\b", declaration):
        return None
    before_initializer = declaration.split("=", 1)[0]
    if "(" in _remove_angle_groups(before_initializer):
        return None

    match = FIELD_PATTERN.match(declaration + ";")
    if not match:
        return None
    return Field(
        name=match.group(2),
        type_name=match.group(1).strip(),
        attrs=attrs,
        access=access,
    )


def parse_fields(lines: list[str], start: int, end: int, default_access: str) -> list[Field]:
    """Parse a type's class-body data members, tracking access and skipping every non-data-member
    declaration. All data members are recorded (not just attributed ones); subsystem ownership and
    the public-by-default serialization decision are left to downstream processors."""

    body = _extract_class_body("\n".join(lines[start : end + 1]))
    fields: list[Field] = []
    access = default_access
    accumulated: list[str] = []
    paren_depth = 0
    index = 0
    length = len(body)
    while index < length:
        character = body[index]
        if character in "\"'":
            close = _skip_string(body, index)
            accumulated.append(body[index:close])
            index = close
            continue
        if body.startswith("//", index):
            newline = body.find("\n", index)
            index = length if newline == -1 else newline
            continue
        if body.startswith("/*", index):
            close = body.find("*/", index + 2)
            index = length if close == -1 else close + 2
            continue
        if body.startswith("[[", index):
            close = body.find("]]", index + 2)
            close = length if close == -1 else close + 2
            accumulated.append(body[index:close])
            index = close
            continue
        if character == "(":
            paren_depth += 1
            accumulated.append(character)
            index += 1
            continue
        if character == ")":
            if paren_depth > 0:
                paren_depth -= 1
            accumulated.append(character)
            index += 1
            continue
        if character == "{" and paren_depth == 0:
            # A class-body brace ends the current declaration: it is either a data member's braced
            # initializer, a function body, or a nested type body. The text before the brace decides;
            # in every case the brace block (and any trailing ';') is consumed here.
            head = "".join(accumulated)
            after_block = _skip_block(body, index)
            cursor = after_block
            while cursor < length and body[cursor] in " \t\r\n":
                cursor += 1
            if cursor < length and body[cursor] == ";":
                cursor += 1
            field = _make_field_from_head(head, access)
            if field is not None:
                fields.append(field)
            accumulated = []
            index = cursor
            continue
        if character == "{":
            # Brace inside a parameter list (e.g. a default argument) — keep it balanced verbatim.
            after_block = _skip_block(body, index)
            accumulated.append(body[index:after_block])
            index = after_block
            continue
        if character == ";" and paren_depth == 0:
            field = _make_field_from_head("".join(accumulated), access)
            if field is not None:
                fields.append(field)
            accumulated = []
            index += 1
            continue
        if character == ":" and paren_depth == 0:
            token = re.sub(r"\s+", " ", remove_attributes("".join(accumulated))).strip()
            if token in ACCESS_LABELS:
                access = token
                accumulated = []
                index += 1
                continue
            accumulated.append(character)
            index += 1
            continue
        accumulated.append(character)
        index += 1

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
    # A `template <...>` header (and any `requires` clause) sits on its own line(s) before the struct
    # it templates. It is remembered here so the following declaration can carry its parameter list.
    pending_template = ""
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

        template_match = TEMPLATE_PATTERN.match(without_attrs)
        if template_match is not None:
            pending_template = template_match.group(1).strip()
            pending_attrs.extend(attrs)
            index += 1
            continue
        # A constraint clause continues the pending template header; skip it but keep the header so the
        # struct that follows still picks up its parameter list.
        if pending_template and without_attrs.startswith("requires"):
            index += 1
            continue

        active_attrs = pending_attrs + attrs
        pending_attrs = []
        # The template header only applies to the immediately following declaration; consume it here so
        # a stray header before a function or unrelated line never leaks onto a later struct.
        active_template = pending_template
        pending_template = ""

        type_match = TYPE_PATTERN.match(without_attrs)
        if type_match:
            end = find_matching_type_end(lines, index)
            type_name = type_match.group(3)
            default_access = "public" if type_match.group(1) == "struct" else "private"
            fields = parse_fields(lines, index, end, default_access)
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
                        template_params=active_template,
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
