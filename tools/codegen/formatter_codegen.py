from __future__ import annotations

from model import CodegenError, SerializableType, attr_arg, attr_list_arg, cpp_string, find_attr, qualified_name


def make_value_expression(argument: str) -> str:
    if "$" in argument:
        return argument.replace("$", "value")
    return f"value.{argument}"


def emit_formatter_declaration(type_info: SerializableType) -> list[str]:
    attr = find_attr(type_info.attrs, "printable")
    if attr is None:
        return []
    format_text = attr_arg(attr, 0, "format")
    fields = attr_list_arg(attr, "fields") or attr.args[1:]
    if type_info.declaration_kind != "enum" and format_text is None:
        raise CodegenError(f"{type_info.name} requires a format string for [[tbx::printable]].")
    if type_info.declaration_kind != "enum" and not fields:
        raise CodegenError(f"{type_info.name} requires at least one field for [[tbx::printable]].")

    qualified = qualified_name(type_info)
    source_path = type_info.source_path.replace("\\", "/")
    api_macro = (
        type_info.api_macro
        or ("TBX_API" if type_info.namespace == "tbx" and "/engine/include/" in source_path else "")
    )
    api_prefix = f"{api_macro} " if api_macro else ""
    return [
        "template <>",
        f"struct std::formatter<{qualified}>",
        "{",
        "    constexpr auto parse(std::format_parse_context& ctx)",
        "    {",
        "        return _formatter.parse(ctx);",
        "    }",
        "",
        f"    {api_prefix}auto format(const {qualified}& value, std::format_context& ctx) const",
        "        -> std::format_context::iterator;",
        "",
        "    std::formatter<std::string> _formatter;",
        "};",
        "",
    ]


def emit_enum_name_formatter(type_info: SerializableType) -> list[str]:
    qualified = qualified_name(type_info)
    lines = [
        f"auto std::formatter<{qualified}>::format(const {qualified}& value, std::format_context& ctx) const",
        "    -> std::format_context::iterator",
        "{",
        "    auto name = std::string_view(\"(unknown)\");",
        "    switch (value)",
        "    {",
    ]
    for enum_value in type_info.enum_values:
        enum_value_name = (
            f"{qualified}::{enum_value.name}"
            if type_info.enum_scoped
            else f"{type_info.namespace}::{enum_value.name}" if type_info.namespace else enum_value.name
        )
        lines.extend(
            [
                f"        case {enum_value_name}:",
                f"            name = {cpp_string(enum_value.json_name)};",
                "            break;",
            ]
        )

    lines.extend(
        [
            "        default:",
            "            break;",
            "    }",
            "",
            "    return _formatter.format(std::string(name), ctx);",
            "}",
            "",
        ]
    )
    return lines


def emit_formatter(type_info: SerializableType) -> list[str]:
    attr = find_attr(type_info.attrs, "printable")
    if attr is None:
        return []
    if type_info.declaration_kind == "enum" and not attr.args and not attr.named_args:
        return emit_enum_name_formatter(type_info)
    format_text = attr_arg(attr, 0, "format")
    if format_text is None:
        raise CodegenError(f"{type_info.name} requires a format string for [[tbx::printable]].")

    fields = attr_list_arg(attr, "fields") or attr.args[1:]
    if not fields:
        raise CodegenError(f"{type_info.name} requires at least one field for [[tbx::printable]].")

    qualified = qualified_name(type_info)
    value_args = ",\n            ".join(make_value_expression(field) for field in fields)
    return [
        f"auto std::formatter<{qualified}>::format(const {qualified}& value, std::format_context& ctx) const",
        "    -> std::format_context::iterator",
        "{",
        "    return _formatter.format(",
        "        std::format(",
        f"            {cpp_string(format_text)},",
        f"            {value_args}),",
        "        ctx);",
        "}",
        "",
    ]
