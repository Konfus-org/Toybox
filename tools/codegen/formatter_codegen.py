"""std::formatter glue — rendered through the C++ template pack (``templates/cpp/formatter_*``)."""

from __future__ import annotations

from model import CodegenError, SerializableType, attr_arg, attr_list_arg, cpp_string, find_attr, qualified_name
from render import render_lines


def make_value_expression(argument: str) -> str:
    if "$" in argument:
        return argument.replace("$", "value")
    return f"value.{argument}"


def _derived_api_prefix(type_info: SerializableType) -> str:
    """The export macro, falling back to TBX_API for engine-include types in namespace tbx."""
    source_path = type_info.source_path.replace("\\", "/")
    api_macro = type_info.api_macro or (
        "TBX_API" if type_info.namespace == "tbx" and "/engine/include/" in source_path else ""
    )
    return f"{api_macro} " if api_macro else ""


def _enum_labels(type_info: SerializableType) -> list[dict[str, str]]:
    labels: list[dict[str, str]] = []
    for enum_value in type_info.enum_values:
        if type_info.enum_scoped:
            label = f"{qualified_name(type_info)}::{enum_value.name}"
        elif type_info.namespace:
            label = f"{type_info.namespace}::{enum_value.name}"
        else:
            label = enum_value.name
        labels.append({"label": label, "json": cpp_string(enum_value.json_name)})
    return labels


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

    return render_lines(
        "cpp/formatter_declaration.jinja",
        qualified=qualified_name(type_info),
        api_prefix=_derived_api_prefix(type_info),
    )


def emit_enum_name_formatter(type_info: SerializableType) -> list[str]:
    return render_lines(
        "cpp/formatter_enum_definition.jinja",
        qualified=qualified_name(type_info),
        values=_enum_labels(type_info),
    )


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

    value_args = ",\n            ".join(make_value_expression(field) for field in fields)
    return render_lines(
        "cpp/formatter_definition.jinja",
        qualified=qualified_name(type_info),
        format_text=cpp_string(format_text),
        value_args=value_args,
    )
