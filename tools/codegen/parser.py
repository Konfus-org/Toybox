"""C++ metadata parser for Toybox code generation.

Structure (Layer 2 of the codegen: Parser / Normalizer):
  * a dependency-free tokenizer (``cpp_lexer``) delimits declarations robustly across comments,
    string/char/raw literals, preprocessor lines and balanced brackets;
  * a line-anchored walker over those tokens finds top-level namespaces, records, enums and aliases;
  * each discovered declaration's body/header is classified by small text sub-parsers.

It recognizes declarations well enough to build the neutral IR (``model``) and leaves every behavior
decision to downstream processors.
"""

from __future__ import annotations

import re

from cpp_lexer import ATTR, EOF, ID, PUNCT, Token, tokenize
from model import (
    Attribute,
    EnumValue,
    Field,
    SerializableType,
    attr_value,
    split_attribute_values,
)


# --------------------------------------------------------------------------------------------------
# Attribute parsing (text level).
# --------------------------------------------------------------------------------------------------

# Toybox attributes live in the `tbx::` attribute namespace. Engine code (inside `namespace tbx`)
# writes the bare name, e.g. [[serializable]]; plugins/examples write [[tbx::serializable]]. Both
# forms normalize to the same captured name.
# DOTALL so an attribute's argument list may span multiple source lines (the tokenizer keeps a
# multi-line ``[[ ... ]]`` block as one token; the legacy scanner instead pre-collapsed newlines).
ATTRIBUTE_PATTERN = re.compile(r"\[\[\s*(?:tbx::)?([A-Za-z_]\w*)\s*(?:\((.*?)\))?\s*\]\]", re.DOTALL)
ENUM_VALUE_PATTERN = re.compile(r"^\s*([A-Za-z_]\w*)\s*(.*?)(?:,|$)")
EQUALITY_OPERATOR_PATTERN = re.compile(r"\boperator\s*==")
SERIALIZER_PATTERN = re.compile(
    r"\bstruct\s+(?:(?:[A-Za-z_]\w*_API|TBX_API)\s+)?Serializer\s*<\s*([A-Za-z_]\w*)\s*>"
)
FIELD_PATTERN = re.compile(
    r"^\s*(?:(?:static|inline|constexpr|const)\s+)*(.+?)\s+([A-Za-z_]\w*)"
    r"(?:\s*(?:=|\{).*)?;\s*$"
)

ACCESS_LABELS = {"public", "private", "protected"}
# Leading keywords that mark a class-body declaration as something other than a serializable data
# member (type aliases, friends, statics, function specifiers, nested types, templates).
_NON_MEMBER_LEADING = re.compile(
    r"^(?:using|typedef|friend|static|constexpr|consteval|constinit|inline|virtual|explicit|"
    r"template|struct|class|union|enum)\b"
)
# Identifiers accepted between the record keyword and its name as an export/API macro.
_API_MACRO = re.compile(r"^(?:[A-Za-z_]\w*_API|TBX_API)$")


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


def _attr_from_token(token: Token) -> list[Attribute]:
    return parse_attributes(f"[[{token.value}]]")


# --------------------------------------------------------------------------------------------------
# Body sub-parsers (operate on the exact source text of a type body).
# --------------------------------------------------------------------------------------------------

def _skip_string(text: str, index: int) -> int:
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


def _remove_angle_groups(text: str) -> str:
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
    return Field(name=match.group(2), type_name=match.group(1).strip(), attrs=attrs, access=access)


def parse_fields_in_body(body: str, default_access: str) -> list[Field]:
    """Parse a type's data members from the exact text between its outermost braces, tracking access
    and skipping every non-data-member declaration. All data members are recorded (not just attributed
    ones); ownership and the public-by-default serialization decision are left to processors."""
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


def parse_enum_values_in_body(body: str) -> list[EnumValue]:
    values: list[EnumValue] = []
    for line in body.splitlines():
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


# --------------------------------------------------------------------------------------------------
# Token walker (top-level declaration discovery).
# --------------------------------------------------------------------------------------------------

_RECORD_KEYWORDS = {"struct", "class"}


def _line_tokens(tokens: list[Token], start: int) -> tuple[list[Token], int]:
    """Return the tokens on the same source line as ``tokens[start]`` and the index of the next line."""
    line = tokens[start].line
    index = start
    collected: list[Token] = []
    while tokens[index].kind != EOF and tokens[index].line == line:
        collected.append(tokens[index])
        index += 1
    return collected, index


def _match_braces(tokens: list[Token], open_index: int) -> int:
    """Given the index of a ``{`` token, return the index of its matching ``}`` (or EOF index)."""
    depth = 0
    index = open_index
    while tokens[index].kind != EOF:
        token = tokens[index]
        if token.kind == PUNCT and token.value == "{":
            depth += 1
        elif token.kind == PUNCT and token.value == "}":
            depth -= 1
            if depth == 0:
                return index
        index += 1
    return index


def _find_body_open(tokens: list[Token], start: int) -> int | None:
    """From ``start``, return the index of the record/enum body-opening ``{``, or None if a top-level
    ``;`` (a forward declaration) is reached first."""
    index = start
    while tokens[index].kind != EOF:
        token = tokens[index]
        if token.kind == PUNCT and token.value == "{":
            return index
        if token.kind == PUNCT and token.value == ";":
            return None
        index += 1
    return None


def _next_line_index(tokens: list[Token], from_index: int) -> int:
    """Index of the first token on a line after ``tokens[from_index]`` (skipping a trailing ';')."""
    line = tokens[from_index].line
    index = from_index
    while tokens[index].kind != EOF and tokens[index].line == line:
        index += 1
    return index


def _parse_record_header(
    tokens: list[Token], start: int, body_open: int, source: str
) -> tuple[str, str, str]:
    """Extract (api_macro, name, bases) from a record header between the keyword and the body '{'."""
    api_macro = ""
    name = ""
    index = start
    while index < body_open:
        token = tokens[index]
        if token.kind == ID:
            if not name and not api_macro and _API_MACRO.match(token.value):
                api_macro = token.value
                index += 1
                continue
            name = token.value
            index += 1
            break
        index += 1
    bases = ""
    while index < body_open:
        token = tokens[index]
        if token.kind == PUNCT and token.value == ":":
            bases = source[token.end:tokens[body_open].pos].lstrip()
            break
        index += 1
    return api_macro, name, bases


def _parse_enum_header(
    tokens: list[Token], start: int, body_open: int, source: str
) -> tuple[bool, str, str]:
    """Extract (scoped, name, underlying_type) from an enum header."""
    index = start
    scoped = False
    if index < body_open and tokens[index].kind == ID and tokens[index].value == "class":
        scoped = True
        index += 1
    name = ""
    while index < body_open:
        if tokens[index].kind == ID:
            name = tokens[index].value
            index += 1
            break
        index += 1
    underlying = ""
    while index < body_open:
        token = tokens[index]
        if token.kind == PUNCT and token.value == ":":
            underlying = source[token.end:tokens[body_open].pos].strip()
            break
        index += 1
    return scoped, name, underlying


def _parse_using(tokens: list[Token], start: int, source: str) -> tuple[str | None, str, int | None]:
    """Parse a `using Name = value;` alias. Returns (name|None, value, semicolon_index|None)."""
    index = start
    if index >= len(tokens) or tokens[index].kind != ID:
        return None, "", None
    name = tokens[index].value
    index += 1
    if not (index < len(tokens) and tokens[index].kind == PUNCT and tokens[index].value == "="):
        return None, "", None
    eq = index
    while tokens[index].kind != EOF and not (tokens[index].kind == PUNCT and tokens[index].value == ";"):
        index += 1
    if tokens[index].kind == EOF:
        return None, "", None
    # Collapse a multi-line alias to a single spaced line (matching the legacy using-collapse).
    raw_value = source[tokens[eq].end:tokens[index].pos]
    value = " ".join(part.strip() for part in raw_value.splitlines()).strip()
    return name, value, index


def parse_type_declarations(source: str, source_path: str) -> list[SerializableType]:
    """Return declarations carrying passive Toybox metadata. The walker stops at discovery; it does
    not decide which subsystem owns an attribute."""
    tokens = tokenize(source)
    serializer_types = {match.group(1) for match in SERIALIZER_PATTERN.finditer(source)}
    types: list[SerializableType] = []

    namespace = ""
    pending_attrs: list[Attribute] = []
    pending_template = ""

    index = 0
    total = len(tokens)
    while tokens[index].kind != EOF:
        line, next_line = _line_tokens(tokens, index)

        lead = 0
        while lead < len(line) and line[lead].kind == ATTR:
            lead += 1
        rest = line[lead:]
        line_attrs: list[Attribute] = [a for token in line if token.kind == ATTR for a in _attr_from_token(token)]

        # Attribute-only line (optionally a trailing ';'): accumulate and move on.
        if not rest or (len(rest) == 1 and rest[0].kind == PUNCT and rest[0].value == ";"):
            pending_attrs.extend(line_attrs)
            index = next_line
            continue

        head = rest[0]

        if head.kind == ID and head.value == "namespace":
            parts = [t.value for t in rest[1:] if t.kind == ID]
            if parts:
                namespace = "::".join(parts)
            pending_attrs = []
            pending_template = ""
            index = next_line
            continue

        if head.kind == ID and head.value == "template":
            head_index = index + lead
            angle = head_index + 1
            if angle < total and tokens[angle].kind == PUNCT and tokens[angle].value == "<":
                depth = 0
                cursor = angle
                while tokens[cursor].kind != EOF:
                    if tokens[cursor].kind == PUNCT and tokens[cursor].value == "<":
                        depth += 1
                    elif tokens[cursor].kind == PUNCT and tokens[cursor].value == ">":
                        depth -= 1
                        if depth == 0:
                            break
                    cursor += 1
                close = cursor
                after = close + 1
                # Only a bare `template <...>` line (nothing after the '>' on that line) carries forward.
                if tokens[close].kind != EOF and after < total and tokens[after].line == tokens[close].line:
                    pending_attrs = []
                    pending_template = ""
                    index = next_line
                    continue
                pending_template = source[tokens[angle].end:tokens[close].pos].strip()
                pending_attrs.extend(line_attrs)
                index = next_line
                continue
            pending_attrs = []
            pending_template = ""
            index = next_line
            continue

        if head.kind == ID and head.value == "requires" and pending_template:
            index = next_line
            continue

        active_attrs = pending_attrs + line_attrs
        head_index = index + lead

        if head.kind == ID and head.value in _RECORD_KEYWORDS:
            body_open = _find_body_open(tokens, head_index + 1)
            if body_open is None:  # forward declaration, not a definition
                pending_attrs = []
                pending_template = ""
                index = next_line
                continue
            body_close = _match_braces(tokens, body_open)
            declaration_kind = head.value
            default_access = "public" if declaration_kind == "struct" else "private"
            api_macro, name, bases = _parse_record_header(tokens, head_index + 1, body_open, source)
            body_text = source[tokens[body_open].end:tokens[body_close].pos]
            fields = parse_fields_in_body(body_text, default_access)
            if active_attrs or fields:
                types.append(
                    SerializableType(
                        namespace=namespace,
                        name=name,
                        declaration_kind=declaration_kind,
                        attrs=active_attrs,
                        api_macro=api_macro,
                        bases=bases,
                        fields=fields,
                        template_params=pending_template,
                        has_serializer=name in serializer_types,
                        has_equality_operator=bool(EQUALITY_OPERATOR_PATTERN.search(body_text)),
                        source_path=source_path,
                        line=head.line,
                    )
                )
            pending_attrs = []
            pending_template = ""
            index = _next_line_index(tokens, body_close)
            continue

        if head.kind == ID and head.value == "enum":
            body_open = _find_body_open(tokens, head_index + 1)
            if body_open is None:
                pending_attrs = []
                pending_template = ""
                index = next_line
                continue
            body_close = _match_braces(tokens, body_open)
            scoped, name, underlying = _parse_enum_header(tokens, head_index + 1, body_open, source)
            if active_attrs:
                body_text = source[tokens[body_open].end:tokens[body_close].pos]
                types.append(
                    SerializableType(
                        namespace=namespace,
                        name=name,
                        declaration_kind="enum",
                        attrs=active_attrs,
                        enum_values=parse_enum_values_in_body(body_text),
                        enum_scoped=scoped,
                        enum_underlying_type=underlying,
                        source_path=source_path,
                        line=head.line,
                    )
                )
            pending_attrs = []
            pending_template = ""
            index = _next_line_index(tokens, body_close)
            continue

        if head.kind == ID and head.value == "using":
            alias_name, alias_value, semi_index = _parse_using(tokens, head_index + 1, source)
            if alias_name is not None and active_attrs:
                types.append(
                    SerializableType(
                        namespace=namespace,
                        name=alias_name,
                        declaration_kind="using",
                        attrs=active_attrs,
                        alias_value=alias_value,
                        source_path=source_path,
                        line=head.line,
                    )
                )
            pending_attrs = []
            pending_template = ""
            index = next_line if semi_index is None else _next_line_index(tokens, semi_index)
            continue

        # Any other content line: drop pending metadata and advance a line.
        pending_attrs = []
        pending_template = ""
        index = next_line

    return types


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
