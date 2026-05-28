from __future__ import annotations

from model import CodegenError, SerializableType, cpp_string, find_attr, qualified_name


def make_value_expression(argument: str) -> str:
    if "$" in argument:
        return argument.replace("$", "value")
    return f"value.{argument}"


def emit_enum_name_formatter(type_info: SerializableType) -> list[str]:
    qualified = qualified_name(type_info)
    lines = [
        "template <>",
        f"struct std::formatter<{qualified}>",
        "{",
        "    constexpr auto parse(std::format_parse_context& ctx)",
        "    {",
        "        return _formatter.parse(ctx);",
        "    }",
        "",
        "    template <typename TFormatContext>",
        f"    auto format(const {qualified}& value, TFormatContext& ctx) const",
        "    {",
        "        auto name = std::string_view(\"(unknown)\");",
        "        switch (value)",
        "        {",
    ]
    for enum_value in type_info.enum_values:
        lines.extend(
            [
                f"            case {qualified}::{enum_value.name}:",
                f"                name = {cpp_string(enum_value.json_name)};",
                "                break;",
            ]
        )

    lines.extend(
        [
            "            default:",
            "                break;",
            "        }",
            "",
            "        return _formatter.format(std::string(name), ctx);",
            "    }",
            "",
            "    std::formatter<std::string> _formatter;",
            "};",
            "",
        ]
    )
    return lines


def emit_formatter(type_info: SerializableType) -> list[str]:
    attr = find_attr(type_info.attrs, "printable")
    if attr is None:
        return []
    if type_info.declaration_kind == "enum" and not attr.args:
        return emit_enum_name_formatter(type_info)
    if len(attr.args) < 1:
        raise CodegenError(f"{type_info.name} requires a format string for [[tbx::printable]].")

    format_text = attr.args[0]
    fields = attr.args[1:]
    if not fields:
        raise CodegenError(f"{type_info.name} requires at least one field for [[tbx::printable]].")

    qualified = qualified_name(type_info)
    value_args = ",\n                ".join(make_value_expression(field) for field in fields)
    return [
        "template <>",
        f"struct std::formatter<{qualified}>",
        "{",
        "    constexpr auto parse(std::format_parse_context& ctx)",
        "    {",
        "        return _formatter.parse(ctx);",
        "    }",
        "",
        "    template <typename TFormatContext>",
        f"    auto format(const {qualified}& value, TFormatContext& ctx) const",
        "    {",
        "        return _formatter.format(",
        "            std::format(",
        f"                {cpp_string(format_text)},",
        f"                {value_args}),",
        "            ctx);",
        "    }",
        "",
        "    std::formatter<std::string> _formatter;",
        "};",
        "",
    ]
