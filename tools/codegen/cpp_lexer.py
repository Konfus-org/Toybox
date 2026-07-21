"""A small, dependency-free C++ lexer for Toybox codegen.

It exists to *delimit* declarations robustly — correctly stepping over comments, string/char/raw
literals, preprocessor lines, and balanced ``<>``/``()``/``{}`` — so the declaration parser never has
to reason about raw text spanning multiple lines. It is deliberately not a full C++ lexer: it produces
just enough token structure for the declaration scanner to find namespaces, types, enums, aliases,
attributes, and data members.
"""

from __future__ import annotations

import dataclasses


# Token kinds.
ATTR = "attr"      # a [[ ... ]] attribute block; value is the inner text (between [[ and ]])
ID = "id"          # identifier or keyword
NUM = "num"        # numeric literal
STR = "str"        # string literal (including raw / prefixed)
CHAR = "char"      # character literal
PUNCT = "punct"    # a punctuator (":" "::" "{" "}" "(" ")" "<" ">" ";" "," "=" ...)
EOF = "eof"


@dataclasses.dataclass
class Token:
    kind: str
    value: str
    line: int
    pos: int = 0      # start offset of the token in the source
    end: int = 0      # offset just past the token in the source


_ID_START = set("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ_")
_ID_CONT = _ID_START | set("0123456789")
_DIGITS = set("0123456789")
# Multi-character punctuators the scanner benefits from seeing as one token.
_TWO_CHAR_PUNCT = {"::", "->", "<=", ">=", "==", "!=", "&&", "||", "<<", ">>", "+=", "-="}


def tokenize(source: str) -> list[Token]:
    tokens: list[Token] = []
    index = 0
    length = len(source)
    line = 1
    at_line_start = True  # tracks whether only whitespace has appeared since the last newline

    def add(kind: str, start: int, stop: int) -> None:
        tokens.append(Token(kind, source[start:stop], line, start, stop))

    while index < length:
        char = source[index]

        # Newlines / whitespace.
        if char == "\n":
            line += 1
            index += 1
            at_line_start = True
            continue
        if char in " \t\r\f\v":
            index += 1
            continue

        # Comments.
        if source.startswith("//", index):
            newline = source.find("\n", index)
            index = length if newline == -1 else newline
            continue
        if source.startswith("/*", index):
            close = source.find("*/", index + 2)
            if close == -1:
                index = length
            else:
                line += source.count("\n", index, close + 2)
                index = close + 2
            continue

        # Preprocessor directive: consume the whole logical line (honoring backslash continuations).
        if char == "#" and at_line_start:
            while index < length:
                if source[index] == "\n":
                    back = index - 1
                    while back >= 0 and source[back] in " \t\r":
                        back -= 1
                    if back >= 0 and source[back] == "\\":
                        line += 1
                        index += 1
                        continue
                    break
                index += 1
            continue

        at_line_start = False

        # Attribute block [[ ... ]] — match the FIRST closing ]] (mirrors the legacy scanner).
        if source.startswith("[[", index):
            close = source.find("]]", index + 2)
            if close == -1:
                tokens.append(Token(ATTR, source[index + 2:], line, index, length))
                line += source.count("\n", index, length)
                index = length
                continue
            tokens.append(Token(ATTR, source[index + 2:close], line, index, close + 2))
            line += source.count("\n", index, close + 2)
            index = close + 2
            continue

        # Raw string literal: (prefix)? R"delim( ... )delim"
        raw_prefix_len = _raw_string_prefix_len(source, index)
        if raw_prefix_len:
            end, consumed_lines = _scan_raw_string(source, index, raw_prefix_len)
            add(STR, index, end)
            line += consumed_lines
            index = end
            continue

        # String / char literals (with optional u8/u/U/L prefix).
        literal_prefix = _literal_prefix_len(source, index)
        if literal_prefix is not None:
            quote_index = index + literal_prefix
            quote = source[quote_index]
            end, consumed_lines = _scan_quoted(source, quote_index, quote)
            tokens.append(Token(STR if quote == '"' else CHAR, source[index:end], line, index, end))
            line += consumed_lines
            index = end
            continue

        # Identifiers / keywords.
        if char in _ID_START:
            start = index
            index += 1
            while index < length and source[index] in _ID_CONT:
                index += 1
            add(ID, start, index)
            continue

        # Numeric literals (kept opaque; only their presence matters to the scanner).
        if char in _DIGITS or (char == "." and index + 1 < length and source[index + 1] in _DIGITS):
            start = index
            index += 1
            while index < length and (source[index] in _ID_CONT or source[index] in ".'"):
                index += 1
            add(NUM, start, index)
            continue

        # Punctuators.
        if source[index:index + 2] in _TWO_CHAR_PUNCT:
            add(PUNCT, index, index + 2)
            index += 2
            continue
        add(PUNCT, index, index + 1)
        index += 1

    tokens.append(Token(EOF, "", line, length, length))
    return tokens


def _literal_prefix_len(source: str, index: int) -> int | None:
    """Return the offset to the opening quote for a (possibly prefixed) string/char literal, else None."""
    if source[index] in "\"'":
        return 0
    for prefix in ("u8", "u", "U", "L"):
        if source.startswith(prefix, index):
            after = index + len(prefix)
            if after < len(source) and source[after] in "\"'":
                return len(prefix)
    return None


def _raw_string_prefix_len(source: str, index: int) -> int:
    """Return the length of a raw-string opener prefix ending in R" (e.g. R", u8R", LR"), else 0."""
    for prefix in ("u8R", "uR", "UR", "LR", "R"):
        if source.startswith(prefix, index):
            after = index + len(prefix)
            if after < len(source) and source[after] == '"':
                return len(prefix)
    return 0


def _scan_raw_string(source: str, index: int, prefix_len: int) -> tuple[int, int]:
    quote = index + prefix_len
    delim_start = quote + 1
    paren = source.find("(", delim_start)
    if paren == -1:
        return len(source), source.count("\n", index)
    delimiter = source[delim_start:paren]
    closer = ")" + delimiter + '"'
    close = source.find(closer, paren + 1)
    if close == -1:
        return len(source), source.count("\n", index)
    end = close + len(closer)
    return end, source.count("\n", index, end)


def _scan_quoted(source: str, index: int, quote: str) -> tuple[int, int]:
    start = index
    index += 1
    length = len(source)
    while index < length:
        char = source[index]
        if char == "\\":
            index += 2
            continue
        if char == "\n":  # unterminated; stop at line end
            return index, source.count("\n", start, index)
        if char == quote:
            index += 1
            return index, source.count("\n", start, index)
        index += 1
    return index, source.count("\n", start, index)
