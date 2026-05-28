from __future__ import annotations

from model import CodegenError, SerializableType, find_attr, qualified_name


def make_value_expression(argument: str) -> str:
    if "$" in argument:
        return argument.replace("$", "value")
    return f"value.{argument}"


def emit_single_hash_return(argument: str) -> str:
    expression = make_value_expression(argument)
    if "$" in argument:
        return f"        return static_cast<::size>({expression});"
    return f"        return static_cast<::size>(std::hash<decltype({expression})>()({expression}));"


def emit_hash(type_info: SerializableType) -> list[str]:
    attr = find_attr(type_info.attrs, "hash")
    if attr is None:
        return []
    if not attr.args:
        raise CodegenError(f"{type_info.name} requires at least one field for [[tbx::hash]].")

    qualified = qualified_name(type_info)
    if len(attr.args) == 1:
        return [
            "template <>",
            f"struct std::hash<{qualified}>",
            "{",
            f"    ::size operator()(const {qualified}& value) const",
            "    {",
            emit_single_hash_return(attr.args[0]),
            "    }",
            "};",
            "",
        ]

    lines = [
        "template <>",
        f"struct std::hash<{qualified}>",
        "{",
        f"    ::size operator()(const {qualified}& value) const",
        "    {",
        "        auto seed = ::tbx::TBX_FNV1A_OFFSET_BASIS;",
    ]
    for field in attr.args:
        lines.append(f"        seed = ::tbx::hash_combine(seed, {make_value_expression(field)});")
    lines.extend(
        [
            "        return static_cast<::size>(seed);",
            "    }",
            "};",
            "",
        ]
    )
    return lines
