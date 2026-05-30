from __future__ import annotations

from model import CodegenError, SerializableType, find_attr, qualified_name


def make_value_expression(argument: str, value_name: str = "value") -> str:
    if "$" in argument:
        return argument.replace("$", value_name)
    return f"{value_name}.{argument}"


def make_equality_expression(argument: str) -> str:
    return f"({make_value_expression(argument, 'left')}) == ({make_value_expression(argument, 'right')})"


def emit_single_hash_return(argument: str) -> str:
    expression = make_value_expression(argument)
    return "\n".join(
        [
            "    auto seed = ::tbx::TBX_FNV1A_OFFSET_BASIS;",
            f"    seed = ::tbx::hash_combine(seed, {expression});",
            "    return static_cast<::size>(seed);",
        ]
    )


def emit_variant_hash_return() -> str:
    return "\n".join(
        [
            "    auto seed = ::tbx::TBX_FNV1A_OFFSET_BASIS;",
            "    seed = ::tbx::hash_combine(seed, value.index());",
            "    std::visit(",
            "        [&seed](const auto& tbx_variant_value)",
            "        {",
            "            seed = ::tbx::hash_combine(seed, tbx_variant_value);",
            "        },",
            "        value);",
            "    return static_cast<::size>(seed);",
        ]
    )


def emit_hash_declaration(type_info: SerializableType) -> list[str]:
    attr = find_attr(type_info.attrs, "hash")
    if attr is None:
        return []
    if not attr.args:
        raise CodegenError(f"{type_info.name} requires at least one field for [[tbx::hash]].")

    qualified = qualified_name(type_info)
    source_path = type_info.source_path.replace("\\", "/")
    api_macro = (
        type_info.api_macro
        or ("TBX_API" if type_info.namespace == "tbx" and "/engine/include/" in source_path else "")
    )
    api_prefix = f"{api_macro} " if api_macro else ""
    return [
        "template <>",
        f"struct std::hash<{qualified}>",
        "{",
        f"    {api_prefix}::size operator()(const {qualified}& value) const;",
        "};",
        "",
    ]


def emit_hash_equality_declaration(type_info: SerializableType) -> list[str]:
    attr = find_attr(type_info.attrs, "hash")
    if attr is None or type_info.has_equality_operator or type_info.declaration_kind == "using":
        return []
    if not attr.args:
        raise CodegenError(f"{type_info.name} requires at least one field for [[tbx::hash]].")

    return [
        f"bool operator==(const {type_info.name}& left, const {type_info.name}& right);",
        "",
    ]


def emit_hash_equality(type_info: SerializableType) -> list[str]:
    attr = find_attr(type_info.attrs, "hash")
    if attr is None or type_info.has_equality_operator or type_info.declaration_kind == "using":
        return []
    if not attr.args:
        raise CodegenError(f"{type_info.name} requires at least one field for [[tbx::hash]].")

    comparisons = " && ".join(f"({make_equality_expression(field)})" for field in attr.args)
    return [
        f"bool operator==(const {type_info.name}& left, const {type_info.name}& right)",
        "{",
        f"    return {comparisons};",
        "}",
        "",
    ]


def emit_hash(type_info: SerializableType) -> list[str]:
    attr = find_attr(type_info.attrs, "hash")
    if attr is None:
        return []
    if not attr.args:
        raise CodegenError(f"{type_info.name} requires at least one field for [[tbx::hash]].")

    qualified = qualified_name(type_info)
    lines = [
        f"::size std::hash<{qualified}>::operator()(const {qualified}& value) const",
        "{",
    ]
    if len(attr.args) == 1:
        if (
            attr.args[0].strip() == "$"
            and type_info.declaration_kind == "using"
            and "variant" in type_info.alias_value
        ):
            lines.append(emit_variant_hash_return())
        else:
            lines.append(emit_single_hash_return(attr.args[0]))
    else:
        lines.append("    auto seed = ::tbx::TBX_FNV1A_OFFSET_BASIS;")
        for field in attr.args:
            lines.append(f"    seed = ::tbx::hash_combine(seed, {make_value_expression(field)});")
        lines.append("    return static_cast<::size>(seed);")
    lines.extend(["}", ""])
    return lines
