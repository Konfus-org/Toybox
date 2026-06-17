from __future__ import annotations

from model import SerializableType, cpp_string, type_name, type_version


def emit_type_name(type_info: SerializableType) -> list[str]:
    return [
        f"inline constexpr std::string_view tbx_serialization_type_name(const {type_info.name}*)",
        "{",
        f"    return {cpp_string(type_name(type_info))};",
        "}",
        "",
    ]


def emit_version(type_info: SerializableType) -> list[str]:
    version = type_version(type_info)
    if version is None:
        return []
    return [
        f"inline std::true_type tbx_has_serialization_version(const {type_info.name}*)",
        "{",
        "    return {};",
        "}",
        f"inline std::integral_constant<uint32, {version}> tbx_serialization_version(const {type_info.name}*)",
        "{",
        "    return {};",
        "}",
        "",
    ]
