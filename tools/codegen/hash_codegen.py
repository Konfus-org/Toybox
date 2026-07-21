"""std::hash / equality glue — rendered through the C++ template pack (``templates/cpp/hash_*``)."""

from __future__ import annotations

from model import CodegenError, SerializableType, attr_list_arg, find_attr, qualified_name
from render import render_lines


def make_value_expression(argument: str, value_name: str = "value") -> str:
    if "$" in argument:
        return argument.replace("$", value_name)
    return f"{value_name}.{argument}"


def make_equality_expression(argument: str) -> str:
    return f"({make_value_expression(argument, 'left')}) == ({make_value_expression(argument, 'right')})"


def _hash_fields(type_info: SerializableType) -> list[str]:
    attr = find_attr(type_info.attrs, "hash")
    fields = attr_list_arg(attr, "fields") or attr.args
    if not fields:
        raise CodegenError(f"{type_info.name} requires at least one field for [[tbx::hash]].")
    return fields


def _derived_api_prefix(type_info: SerializableType) -> str:
    source_path = type_info.source_path.replace("\\", "/")
    api_macro = type_info.api_macro or (
        "TBX_API" if type_info.namespace == "tbx" and "/engine/include/" in source_path else ""
    )
    return f"{api_macro} " if api_macro else ""


def emit_hash_declaration(type_info: SerializableType) -> list[str]:
    if find_attr(type_info.attrs, "hash") is None:
        return []
    _hash_fields(type_info)
    return render_lines(
        "cpp/hash_declaration.jinja",
        qualified=qualified_name(type_info),
        api_prefix=_derived_api_prefix(type_info),
    )


def emit_hash_equality_declaration(type_info: SerializableType) -> list[str]:
    attr = find_attr(type_info.attrs, "hash")
    if attr is None or type_info.has_equality_operator or type_info.declaration_kind == "using":
        return []
    _hash_fields(type_info)
    return render_lines(
        "cpp/hash_equality_declaration.jinja",
        name=type_info.name,
        api_prefix=f"{type_info.api_macro} " if type_info.api_macro else "",
    )


def emit_hash_equality(type_info: SerializableType) -> list[str]:
    attr = find_attr(type_info.attrs, "hash")
    if attr is None or type_info.has_equality_operator or type_info.declaration_kind == "using":
        return []
    fields = _hash_fields(type_info)
    comparisons = " && ".join(f"({make_equality_expression(field)})" for field in fields)
    return render_lines("cpp/hash_equality_definition.jinja", name=type_info.name, comparisons=comparisons)


def emit_hash(type_info: SerializableType) -> list[str]:
    if find_attr(type_info.attrs, "hash") is None:
        return []
    fields = _hash_fields(type_info)

    context: dict[str, object] = {"qualified": qualified_name(type_info)}
    if len(fields) == 1:
        if (
            fields[0].strip() == "$"
            and type_info.declaration_kind == "using"
            and "variant" in type_info.alias_value
        ):
            context["mode"] = "variant"
        else:
            context["mode"] = "single"
            context["single_expression"] = make_value_expression(fields[0])
    else:
        context["mode"] = "multi"
        context["field_expressions"] = [make_value_expression(field) for field in fields]
    return render_lines("cpp/hash_definition.jinja", **context)
