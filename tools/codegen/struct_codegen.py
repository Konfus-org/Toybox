"""Struct / value-type serialization glue.

Large boilerplate (serialize/deserialize bodies, registration, indexed, template-value) is rendered
through the C++ template pack (``templates/cpp/struct_*``). Genuine logic — lifecycle-hook discovery,
the private-member access broker's indenting, and the public/non-public dispatch — stays in Python.
"""

from __future__ import annotations

from model import (
    CodegenError,
    Field,
    SerializableType,
    attr_arg,
    cpp_string,
    find_attr,
    json_key,
    template_parameter_names,
)
from render import render_lines

_LIFECYCLE_HOOKS = [
    ("pre_serialize", "pre_serialize", True),
    ("post_serialize", "post_serialize", True),
    ("pre_deserialize", "pre_deserialize", False),
    ("post_deserialize", "post_deserialize", False),
]


def _derived_api_prefix(type_info: SerializableType) -> str:
    """The export macro, defaulting to TBX_API for engine-include types in namespace tbx."""
    source_path = type_info.source_path.replace("\\", "/")
    api_macro = type_info.api_macro or (
        "TBX_API" if type_info.namespace == "tbx" and "/engine/include/" in source_path else ""
    )
    return f"{api_macro} " if api_macro else ""


def _field_context(field: Field) -> dict[str, str]:
    return {"name": field.name, "key": cpp_string(json_key(field))}


def emit_typed_write_field(field: Field, with_default: bool = False) -> list[str]:
    """One write_serialization_field call (kept as a fragment helper; also used by the script path)."""
    lines = [
        "    ::tbx::write_serialization_field(",
        "        tbx_json,",
        f"        {cpp_string(json_key(field))},",
        f"        tbx_value.{field.name}" + ("," if with_default else ");"),
    ]
    if with_default:
        lines.append(f"        tbx_default_value.{field.name});")
    return lines


def _present_hooks(type_info: SerializableType) -> list[tuple[str, str, bool]]:
    hooks: list[tuple[str, str, bool]] = []
    for attribute_name, hook_name, is_const in _LIFECYCLE_HOOKS:
        attribute = find_attr(type_info.attrs, attribute_name)
        if attribute is None:
            continue
        callable_name = attr_arg(attribute, 0, "method")
        if callable_name is None or len(attribute.args) > 1:
            raise CodegenError(
                f"{type_info.name} uses [[tbx::{attribute_name}]] with an invalid argument list."
            )
        hooks.append((hook_name, callable_name.strip(), is_const))
    return hooks


def emit_lifecycle_hook_declarations(type_info: SerializableType) -> list[str]:
    lines: list[str] = []
    for hook_name, _callable, is_const in _present_hooks(type_info):
        qualifier = "const " if is_const else ""
        lines.append(f"void {hook_name}({qualifier}{type_info.name}& tbx_value);")
    if lines:
        lines.append("")
    return lines


def emit_lifecycle_hook_definitions(type_info: SerializableType) -> list[str]:
    lines: list[str] = []
    for hook_name, callable_name, is_const in _present_hooks(type_info):
        qualifier = "const " if is_const else ""
        lines.extend([f"void {hook_name}({qualifier}{type_info.name}& tbx_value)", "{"])
        if "::" in callable_name or "(" in callable_name:
            lines.append(f"    {callable_name}(tbx_value);")
        else:
            lines.append(f"    tbx_value.{callable_name}();")
        lines.extend(["}", ""])
    return lines


def emit_json_function_declarations(type_info: SerializableType) -> list[str]:
    return render_lines(
        "cpp/struct_json_declarations.jinja",
        name=type_info.name,
        api_prefix=_derived_api_prefix(type_info),
    )


def emit_json_function_definitions(
    type_info: SerializableType,
    fields: list[Field],
    force_keyed: bool = False,
) -> list[str]:
    """Emit the serialize/deserialize pair. A single-field value type serializes as its bare field
    value (the fast path components like Tag rely on); force_keyed keeps a keyed document regardless of
    field count (asset bodies, which editors read per-field)."""
    if not fields:
        mode = "empty"
    elif len(fields) == 1 and not force_keyed:
        mode = "single_value"
    else:
        mode = "keyed"
    return render_lines(
        "cpp/struct_json_definitions.jinja",
        name=type_info.name,
        mode=mode,
        fields=[_field_context(field) for field in fields],
        single_keyed=len(fields) == 1,
    )


def emit_template_value_serialization(type_info: SerializableType, field: Field) -> list[str]:
    """Header-only template serialize/deserialize for a single-field template value type (e.g.
    AssetHandle<TAsset>): a transparent passthrough to the sole field, byte-identical on the wire to
    that member. Templates are never runtime-registered, so only the function templates are emitted."""
    parameter_names = template_parameter_names(type_info.template_params)
    return render_lines(
        "cpp/struct_template_value.jinja",
        template_header=f"template <{type_info.template_params}>",
        type_reference=f"{type_info.name}<{', '.join(parameter_names)}>",
        field_name=field.name,
    )


def emit_access_broker_serialization(type_info: SerializableType, fields: list[Field]) -> list[str]:
    """Emit serialize/deserialize for a type with non-public serialized members. The bodies live in a
    ``::tbx::SerializationAccess<T>`` specialization (befriended in-class), and the free functions
    delegate to it. The specialization must be emitted inside ``namespace tbx``."""
    if type_info.namespace != "tbx":
        raise CodegenError(
            f"{type_info.name} has non-public [[tbx::serialize]] members, which are currently only "
            "supported for types in namespace tbx."
        )

    body_functions = emit_json_function_definitions(type_info, fields)
    specialization_body: list[str] = []
    for line in body_functions:
        if line.startswith("void serialize(") or line.startswith("void deserialize("):
            specialization_body.append(f"    static {line}")
        elif line:
            specialization_body.append(f"    {line}")
        else:
            specialization_body.append(line)

    return [
        "template <>",
        f"struct SerializationAccess<{type_info.name}>",
        "{",
        *specialization_body,
        "};",
        "",
        f"void serialize(::tbx::Json& tbx_json, const {type_info.name}& tbx_value)",
        "{",
        f"    ::tbx::SerializationAccess<{type_info.name}>::serialize(tbx_json, tbx_value);",
        "}",
        f"void deserialize(const ::tbx::Json& tbx_json, {type_info.name}& tbx_value)",
        "{",
        f"    ::tbx::SerializationAccess<{type_info.name}>::deserialize(tbx_json, tbx_value);",
        "}",
        "",
    ]


def emit_struct_value_serialization(type_info: SerializableType, fields: list[Field]) -> list[str]:
    """Route through the access broker when any serialized member is non-public; else plain funcs."""
    from model import non_public_serialized_fields

    if non_public_serialized_fields(type_info):
        return emit_access_broker_serialization(type_info, fields)
    return emit_json_function_definitions(type_info, fields)


def emit_struct_serialization_declarations(
    type_info: SerializableType,
    needs_json: bool = True,
) -> list[str]:
    return render_lines(
        "cpp/struct_declarations.jinja",
        name=type_info.name,
        api_prefix=_derived_api_prefix(type_info),
        needs_json=needs_json,
    )


def emit_struct_trait_definition(type_info: SerializableType) -> list[str]:
    return render_lines("cpp/struct_trait.jinja", name=type_info.name)


def emit_serializable_registration(type_info: SerializableType, custom: bool = False) -> list[str]:
    template = "cpp/struct_registration_custom.jinja" if custom else "cpp/struct_registration_normal.jinja"
    return emit_struct_trait_definition(type_info) + render_lines(template, name=type_info.name)


def emit_custom_serializable_registration(
    type_info: SerializableType,
    write_callable: str,
    read_callable: str,
) -> list[str]:
    return emit_struct_trait_definition(type_info) + render_lines(
        "cpp/struct_custom_registration.jinja",
        name=type_info.name,
        write_callable=write_callable,
        read_callable=read_callable,
    )


def emit_indexed(type_info: SerializableType, count: str) -> list[str]:
    return render_lines(
        "cpp/struct_indexed.jinja",
        name=type_info.name,
        count=count,
    ) + emit_serializable_registration(type_info)
