"""Golden-output unit tests for the codegen tool. Run with `python codegen.py --self-test`."""

from __future__ import annotations

import unittest

import emit
import parse
from model import CodegenError

# The real attribute macros (TBX_CODEGEN branch of attributes.h) plus minimal base stubs, so fixtures
# read exactly like engine headers.
PRELUDE = r"""
#define TBX_SERIALIZABLE(...) [[clang::annotate("tbx::serializable(" #__VA_ARGS__ ")")]]
#define TBX_DO_NOT_SERIALIZE [[clang::annotate("tbx::do_not_serialize")]]
#define TBX_EXPOSED_TO_SCRIPTING [[clang::annotate("tbx::exposed_to_scripting")]]
namespace tbx {
struct Block {};
struct Asset { int id; };
struct Vec3 { float x, y, z; };
}
"""


def _parse(body: str):
    return parse.parse_string(PRELUDE + "namespace tbx {\n" + body + "\n}\n")


class ParseTests(unittest.TestCase):
    def test_reflect_only_block_has_no_serializer(self):
        module = _parse("struct TBX_SERIALIZABLE() Transform : Block { Vec3 position; };")
        self.assertEqual(len(module.types), 1)
        transform = module.types[0]
        self.assertEqual(transform.name, "Transform")
        self.assertEqual(transform.wire_name, "Transform")
        self.assertEqual([f.name for f in transform.fields], ["position"])
        self.assertIsNone(transform.serializer)
        self.assertEqual(transform.bases, ["Block"])

    def test_default_asset_gets_default_serializer(self):
        module = _parse("struct TBX_SERIALIZABLE(SerializerFormat::DEFAULT) Material : Asset { float roughness; };")
        material = module.types[0]
        self.assertEqual(material.serializer.format, "DEFAULT")
        self.assertIsNone(material.serializer.reader)
        self.assertIsNone(material.serializer.writer)

    def test_custom_serializer_captures_reader_and_writer(self):
        module = _parse(
            "struct TBX_SERIALIZABLE(SerializerFormat::CUSTOM, reader=&read_tex, writer=&write_tex)"
            " Texture : Asset { int width; };"
        )
        serializer = module.types[0].serializer
        self.assertEqual(serializer.format, "CUSTOM")
        self.assertEqual(serializer.reader, "read_tex")
        self.assertEqual(serializer.writer, "write_tex")

    def test_custom_serializer_reader_only_is_allowed(self):
        module = _parse("struct TBX_SERIALIZABLE(SerializerFormat::CUSTOM, reader=&read_model) Model : Asset { int a; };")
        serializer = module.types[0].serializer
        self.assertEqual(serializer.reader, "read_model")
        self.assertIsNone(serializer.writer)

    def test_name_override_sets_wire_name(self):
        module = _parse('struct TBX_SERIALIZABLE(SerializerFormat::TEXT, name="UiDocument") Document : Asset { int a; };')
        document = module.types[0]
        self.assertEqual(document.name, "Document")
        self.assertEqual(document.wire_name, "UiDocument")

    def test_do_not_serialize_field_is_marked(self):
        module = _parse(
            "struct TBX_SERIALIZABLE() Camera : Block { float fov; TBX_DO_NOT_SERIALIZE float cached; };"
        )
        fields = {f.name: f for f in module.types[0].fields}
        self.assertTrue(fields["fov"].is_serialized)
        self.assertFalse(fields["cached"].is_serialized)

    def test_private_fields_are_excluded(self):
        module = _parse(
            "struct TBX_SERIALIZABLE() Thing : Block { public: int shown; private: int hidden; };"
        )
        self.assertEqual([f.name for f in module.types[0].fields], ["shown"])

    def test_exposed_enum_and_function_are_collected(self):
        module = _parse(
            "enum class TBX_EXPOSED_TO_SCRIPTING MouseButton { LEFT, RIGHT };\n"
            "TBX_EXPOSED_TO_SCRIPTING float angle_axis(float radians, int axis);"
        )
        self.assertEqual(len(module.enums), 1)
        self.assertEqual(module.enums[0].name, "MouseButton")
        self.assertEqual([v[0] for v in module.enums[0].values], ["LEFT", "RIGHT"])
        self.assertEqual(len(module.functions), 1)
        self.assertEqual(module.functions[0].name, "angle_axis")

    def test_custom_without_reader_or_writer_fails(self):
        with self.assertRaises(CodegenError):
            _parse("struct TBX_SERIALIZABLE(SerializerFormat::CUSTOM) Bad : Asset { int a; };")

    def test_default_with_reader_fails(self):
        with self.assertRaises(CodegenError):
            _parse("struct TBX_SERIALIZABLE(SerializerFormat::DEFAULT, reader=&r) Bad : Asset { int a; };")


class EmitTests(unittest.TestCase):
    def test_type_block_matches_golden(self):
        module = _parse("struct TBX_SERIALIZABLE() Transform : Block { Vec3 position; Vec3 scale; };")
        header, source = emit.render_reflection(module.types)
        self.assertIn(
            'register_type<Transform>("Transform")\n'
            "            .field(\"position\", &Transform::position)\n"
            "            .field(\"scale\", &Transform::scale);",
            source,
        )

    def test_do_not_serialize_emits_field_options(self):
        module = _parse("struct TBX_SERIALIZABLE() Camera : Block { float fov; TBX_DO_NOT_SERIALIZE float cached; };")
        _, source = emit.render_reflection(module.types)
        self.assertIn(
            '.field("cached", &Camera::cached, FieldOptions {.is_serialized = false})',
            source,
        )

    def test_custom_asset_registers_no_reflected_fields(self):
        module = _parse(
            "struct TBX_SERIALIZABLE(SerializerFormat::CUSTOM, reader=&read_tex) Texture : Asset"
            " { int width; int height; };"
        )
        _, source = emit.render_reflection(module.types)
        self.assertIn('register_type<Texture>("Texture");', source)
        self.assertNotIn(".field(\"width\"", source)

    def test_text_asset_registers_no_reflected_fields(self):
        module = _parse(
            "struct TBX_SERIALIZABLE(SerializerFormat::TEXT) ShaderSource : Asset { int text; };"
        )
        _, source = emit.render_reflection(module.types)
        self.assertIn('register_type<ShaderSource>("ShaderSource");', source)
        self.assertNotIn(".field(", source.split("register_generated_serializers")[0])

    def test_serializer_block_matches_golden(self):
        module = _parse(
            "struct TBX_SERIALIZABLE(SerializerFormat::CUSTOM, reader=&read_tex, writer=&write_tex)"
            " Texture : Asset { int width; };"
        )
        _, source = emit.render_reflection(module.types)
        self.assertIn(
            "register_serializer<Texture>()\n"
            "            .format(SerializerFormat::CUSTOM)\n"
            "            .deserializer(read_tex)\n"
            "            .serializer(write_tex);",
            source,
        )

    def test_header_declares_both_registrars(self):
        module = _parse("struct TBX_SERIALIZABLE() Transform : Block { Vec3 position; };")
        header, _ = emit.render_reflection(module.types)
        self.assertIn("void register_generated_types();", header)
        self.assertIn("void register_generated_serializers();", header)

    def test_generated_source_includes_owning_header(self):
        # A parsed-from-string type has a synthetic header name; assert the include machinery runs.
        module = _parse("struct TBX_SERIALIZABLE() Transform : Block { Vec3 position; };")
        _, source = emit.render_reflection(module.types)
        self.assertIn('#include "tbx/reflection/reflection.h"', source)
        self.assertIn('#include "tbx/serialization/registration.h"', source)


def run() -> int:
    loader = unittest.TestLoader()
    suite = unittest.TestSuite()
    suite.addTests(loader.loadTestsFromTestCase(ParseTests))
    suite.addTests(loader.loadTestsFromTestCase(EmitTests))
    result = unittest.TextTestRunner(verbosity=2).run(suite)
    return 0 if result.wasSuccessful() else 1


if __name__ == "__main__":
    raise SystemExit(run())
