"""Asset serialization glue (type registration, JSON body, flat .meta sidecar, text/custom bodies) —
rendered through the C++ template pack (``templates/cpp/asset_*`` / ``*_asset*``).

Registration is no longer self-running: each asset type's registration calls are collected into one
explicitly-callable ``register_asset_registrations_{type}(::tbx::RuntimeRegistrations&)`` function that
module aggregators, plugin entries, and app entries invoke."""

from __future__ import annotations

from model import Field, SerializableType, cpp_string, json_key, sanitized_name
from render import render_lines
from struct_codegen import (
    emit_json_function_declarations,
    emit_json_function_definitions,
)


def _field_context(field: Field) -> dict[str, str]:
    return {"name": field.name, "key": cpp_string(json_key(field))}


def _meta_api_prefix(type_info: SerializableType) -> str:
    source_path = type_info.source_path.replace("\\", "/")
    api_macro = type_info.api_macro or (
        "TBX_API" if type_info.namespace == "tbx" and "/engine/include/" in source_path else ""
    )
    return f"{api_macro} " if api_macro else ""


def _meta_helpers(type_info: SerializableType) -> tuple[str, str]:
    key = sanitized_name(type_info.name)
    return f"read_json_asset_meta_{key}", f"write_json_asset_meta_{key}"


def _statement_lines(template_name: str, **context: object) -> list[str]:
    """Render a registration-call statement without the trailing blank line render_lines carries."""
    lines = render_lines(template_name, **context)
    while lines and not lines[-1]:
        lines.pop()
    return lines


def asset_registration_function_name(type_info: SerializableType) -> str:
    return f"register_asset_registrations_{sanitized_name(type_info.name)}"


def emit_asset_type_registration_declarations(type_info: SerializableType) -> list[str]:
    return render_lines("cpp/asset_type_registration_declaration.jinja", name=type_info.name)


def emit_asset_type_registration(type_info: SerializableType) -> list[str]:
    return render_lines("cpp/asset_type_registration.jinja", name=type_info.name)


def emit_asset_type_registration_statement(type_info: SerializableType, version: str) -> list[str]:
    return [f"::tbx::register_asset_type<{type_info.name}>(tbx_runtime, {version});"]


def emit_asset_body_declarations(type_info: SerializableType) -> list[str]:
    return [
        f"std::true_type has_asset_json_fields(const {type_info.name}*);",
        *emit_json_function_declarations(type_info),
    ]


def emit_asset_body(type_info: SerializableType, fields: list[Field]) -> list[str]:
    # head (no trailing blank) + the keyed serialize/deserialize pair.
    head = render_lines("cpp/asset_body_head.jinja", name=type_info.name)[:-1]
    return head + emit_json_function_definitions(type_info, fields, force_keyed=True)


def emit_asset_body_registration_statement(type_info: SerializableType, version: str) -> list[str]:
    return _statement_lines(
        "cpp/asset_body_registration.jinja",
        name=type_info.name,
        version=version,
    )


def emit_asset_meta_declarations(type_info: SerializableType) -> list[str]:
    read_helper, write_helper = _meta_helpers(type_info)
    return render_lines(
        "cpp/asset_meta_declarations.jinja",
        name=type_info.name,
        api_prefix=_meta_api_prefix(type_info),
        read_helper=read_helper,
        write_helper=write_helper,
    )


def emit_asset_meta(type_info: SerializableType, fields: list[Field]) -> list[str]:
    read_helper, write_helper = _meta_helpers(type_info)
    return render_lines(
        "cpp/asset_meta.jinja",
        name=type_info.name,
        read_helper=read_helper,
        write_helper=write_helper,
        fields=[_field_context(field) for field in fields],
    )


def emit_asset_meta_registration_statement(type_info: SerializableType, version: str) -> list[str]:
    read_helper, write_helper = _meta_helpers(type_info)
    return _statement_lines(
        "cpp/asset_meta_registration.jinja",
        name=type_info.name,
        version=version,
        read_helper=read_helper,
        write_helper=write_helper,
    )


def emit_custom_asset_declarations(type_info: SerializableType) -> list[str]:
    return render_lines("cpp/custom_asset_declaration.jinja", name=type_info.name)


def emit_custom_asset(type_info: SerializableType) -> list[str]:
    return render_lines("cpp/custom_asset.jinja", name=type_info.name)


def emit_custom_asset_registration_statement(
    type_info: SerializableType,
    version: str,
    write_callable: str,
    read_callable: str,
) -> list[str]:
    return _statement_lines(
        "cpp/custom_asset_registration.jinja",
        name=type_info.name,
        version=version,
        write_callable=write_callable,
        read_callable=read_callable,
    )


def emit_text_asset_declarations(type_info: SerializableType) -> list[str]:
    return render_lines("cpp/text_asset_declarations.jinja", name=type_info.name)


def emit_text_asset(type_info: SerializableType, field: Field) -> list[str]:
    return render_lines(
        "cpp/text_asset.jinja",
        name=type_info.name,
        field_name=field.name,
    )


def emit_text_asset_registration_statement(
    type_info: SerializableType,
    version: str,
    field: Field,
) -> list[str]:
    return _statement_lines(
        "cpp/text_asset_registration.jinja",
        name=type_info.name,
        version=version,
        field_name=field.name,
    )


def emit_asset_registration_function_declaration(type_info: SerializableType) -> list[str]:
    return [
        f"{_meta_api_prefix(type_info)}bool {asset_registration_function_name(type_info)}("
        "::tbx::RuntimeRegistrations& tbx_runtime);",
        "",
    ]


def emit_asset_registration_function(
    type_info: SerializableType,
    registration_statements: list[list[str]],
) -> list[str]:
    lines = [
        f"bool {asset_registration_function_name(type_info)}(::tbx::RuntimeRegistrations& tbx_runtime)",
        "{",
    ]
    for statement in registration_statements:
        lines.extend(f"    {line}" if line else line for line in statement)
    lines.extend(["    return true;", "}", ""])
    return lines
