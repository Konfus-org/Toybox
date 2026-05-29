from __future__ import annotations

import textwrap
import tempfile
import unittest
from pathlib import Path

from generator import GENERATED_CODE_BANNER, generate_header, generate_source
from model import CodegenError
from parser import parse_source
from resource_codegen import generate_builtin_asset_headers, generate_material_instance_header


class AttributeCodegenTests(unittest.TestCase):
    def generate(self, source: str) -> str:
        return generate_header(parse_source(textwrap.dedent(source)))

    def test_struct_fields_and_name_are_generated(self) -> None:
        output = self.generate(
            """
            namespace tbx::tests
            {
            [[tbx::serializable]];
            [[tbx::name("renamed")]];
            struct Value
            {
                [[tbx::prop]]
                int amount = 0;
            };
            }
            """
        )
        self.assertIn('return "renamed";', output)
        self.assertIn('tbx_json["amount"] = tbx_value.amount;', output)
        self.assertTrue(output.startswith(GENERATED_CODE_BANNER))

    def test_source_stub_is_marked_generated(self) -> None:
        output = generate_source("value.generated.h")

        self.assertTrue(output.startswith(GENERATED_CODE_BANNER))
        self.assertIn("value.generated.h", output)

    def test_plugin_source_generates_fixed_exports(self) -> None:
        types = parse_source(
            textwrap.dedent(
                """
                namespace tbx::tests
                {
                [[tbx::plugin]];
                [[tbx::name("ExamplePlugin")]];
                [[tbx::version("1.2.3")]];
                [[tbx::category("rendering")]];
                [[tbx::priority(25)]];
                [[tbx::dependency("WindowPlugin")]];
                class ExamplePlugin final : public tbx::Plugin
                {
                };
                }
                """
            )
        )

        output = generate_source(
            "example_plugin.generated.h",
            types,
            "tbx/tests/example_plugin.h",
            plugin_abi_version="37",
        )

        self.assertIn("tbx_get_plugin_meta", output)
        self.assertIn("void tbx_get_plugin_meta(::tbx::PluginMeta* out_meta)", output)
        self.assertIn("if (out_meta == nullptr)", output)
        self.assertIn("tbx_create_plugin", output)
        self.assertIn("tbx_destroy_plugin", output)
        self.assertIn('meta.name = "ExamplePlugin";', output)
        self.assertIn('meta.version = "1.2.3";', output)
        self.assertIn("meta.abi_version = 37U;", output)
        self.assertNotIn("::tbx::PluginAbiVersion", output)
        self.assertIn("::tbx::PluginCategory::RENDERING", output)
        self.assertIn('meta.dependencies = {"WindowPlugin"};', output)
        self.assertIn("#if defined(TBX_PLUGIN_RESOURCE_DIRECTORY)", output)
        self.assertIn("meta.resource_directory = TBX_PLUGIN_RESOURCE_DIRECTORY;", output)
        self.assertIn("*out_meta = meta;", output)
        self.assertIn("new tbx::tests::ExamplePlugin()", output)

    def test_resource_codegen_generates_builtin_and_material_headers(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            shader_meta = root / "Shaders" / "Default.vert.meta"
            shader_meta.parent.mkdir(parents=True)
            shader_meta.write_text('{"id": 42, "version": 1}', encoding="utf-8")

            material = root / "Materials" / "Pbr.mat"
            material.parent.mkdir(parents=True)
            material.write_text(
                """
                {
                    "parameters": {"values": [{"name": "albedo_color", "data": {"type": "color"}}]},
                    "textures": {"values": [{"name": "albedo_map"}]}
                }
                """,
                encoding="utf-8",
            )
            material.with_name("Pbr.mat.meta").write_text(
                '{"id": 13, "version": 1}',
                encoding="utf-8",
            )

            builtin_header = root / "generated" / "builtin_assets.generated.h"
            material_header = root / "generated" / "material_descriptions.generated.h"

            generate_builtin_asset_headers(root, builtin_header)
            generate_material_instance_header(root, material_header, "tbx")

            self.assertIn('builtin_assets_shaders.generated.h', builtin_header.read_text(encoding="utf-8"))
            self.assertIn(
                "DefaultVertexShader",
                (root / "generated" / "builtin_assets_shaders.generated.h").read_text(encoding="utf-8"),
            )
            material_output = material_header.read_text(encoding="utf-8")
            self.assertIn("struct PbrMaterial final", material_output)
            self.assertIn('ALBEDO_COLOR = "albedo_color"', material_output)

    def test_asset_body_and_meta_are_generated(self) -> None:
        output = self.generate(
            """
            namespace tbx::tests
            {
            [[tbx::serializable]];
            [[tbx::version(2)]];
            struct Value : Asset
            {
                [[tbx::prop]]
                int amount = 0;
                [[tbx::meta]]
                int import_version = 0;
            };
            }
            """
        )
        self.assertIn("register_asset_body_type<Value>", output)
        self.assertIn("register_asset_meta_type<Value>", output)

    def test_no_semicolon_attribute_blocks_are_generated(self) -> None:
        output = self.generate(
            """
            namespace tbx::tests
            {
            [[tbx::serializable]]
            [[tbx::name("renamed")]]
            struct Value
            {
                [[tbx::prop]]
                int amount = 0;
            };
            }
            """
        )
        self.assertIn('return "renamed";', output)
        self.assertIn('tbx_json["amount"] = tbx_value.amount;', output)

    def test_parent_field_props_are_generated(self) -> None:
        source = textwrap.dedent(
            """
            namespace tbx::tests
            {
            [[tbx::serializable]]
            struct Value : Base
            {
                [[tbx::prop]]
                int amount = 0;
            };
            }
            """
        )
        context = textwrap.dedent(
            """
            namespace tbx::tests
            {
            struct Base
            {
                [[tbx::prop]]
                int id = 0;
            };
            }
            """
        )

        output = generate_header(parse_source(source, context_source=context))

        self.assertIn('tbx_json["id"] = tbx_value.id;', output)
        self.assertIn('tbx_json["amount"] = tbx_value.amount;', output)

    def test_type_level_struct_props_are_rejected(self) -> None:
        with self.assertRaises(CodegenError):
            self.generate(
                """
                namespace tbx::tests
                {
                [[tbx::serializable]]
                [[tbx::prop(id, value)]]
                struct Value
                {
                    int id = 0;
                    int value = 0;
                };
                }
                """
            )

    def test_type_level_alias_props_are_rejected(self) -> None:
        with self.assertRaises(CodegenError):
            self.generate(
                """
                namespace glm
                {
                [[tbx::serializable]]
                [[tbx::prop(x, y, z)]]
                using TbxVec3 = tbx::Vec3;
                }
                """
            )

    def test_type_level_meta_is_rejected(self) -> None:
        with self.assertRaises(CodegenError):
            self.generate(
                """
                namespace tbx::tests
                {
                [[tbx::serializable]]
                [[tbx::version(1)]]
                [[tbx::meta(id)]]
                struct Value : Asset
                {
                    int id = 0;
                };
                }
                """
            )

    def test_parent_props_are_inherited_when_child_has_props(self) -> None:
        source = textwrap.dedent(
            """
            namespace tbx::tests
            {
            [[tbx::serializable]]
            struct Value : Base
            {
                [[tbx::prop]]
                int amount = 0;
            };
            }
            """
        )
        context = textwrap.dedent(
            """
            namespace tbx::tests
            {
            struct Base
            {
                [[tbx::prop]]
                int id = 0;
            };
            }
            """
        )

        output = generate_header(parse_source(source, context_source=context))

        self.assertIn('tbx_json["id"] = tbx_value.id;', output)
        self.assertIn('tbx_json["amount"] = tbx_value.amount;', output)

    def test_script_asset_registration_is_generated(self) -> None:
        output = self.generate(
            """
            namespace tbx::tests
            {
            [[tbx::script]]
            [[tbx::name("door_controller")]]
            [[tbx::version(1)]]
            class DoorController : public tbx::Script
            {
              public:
                [[tbx::prop]]
                float open_speed = 1.0F;

                [[tbx::inject]]
                tbx::ServiceRef<tbx::IInputManager> input = {};
            };
            }
            """
        )
        self.assertIn("register_script_asset_type<DoorController>", output)
        self.assertIn("tbx_json[\"open_speed\"] = tbx_value.open_speed;", output)
        self.assertIn("bind_script_field(tbx_value.open_speed", output)
        self.assertIn("bind_script_field(tbx_value.input", output)
        self.assertNotIn("tbx_json[\"input\"]", output)

    def test_version_only_asset_registers_type_only(self) -> None:
        output = self.generate(
            """
            namespace tbx::tests
            {
            [[tbx::serializable]];
            [[tbx::version(3)]];
            struct Value : Asset
            {
                int amount = 0;
            };
            }
            """
        )
        self.assertIn("register_asset_type<Value>(3)", output)
        self.assertNotIn("register_asset_body_type<Value>", output)
        self.assertNotIn("register_asset_meta_type<Value>", output)

    def test_text_and_meta_asset_are_generated(self) -> None:
        output = self.generate(
            """
            namespace tbx::tests
            {
            [[tbx::serializable]];
            [[tbx::version(4)]];
            struct Value : Asset
            {
                [[tbx::text]]
                std::string source = "";
                [[tbx::meta]]
                int import_version = 0;
            };
            }
            """
        )
        self.assertIn("tbx_has_text_serialization", output)
        self.assertIn("register_asset_body_type<Value>", output)
        self.assertIn("register_asset_meta_type<Value>", output)

    def test_text_and_meta_non_asset_are_generated(self) -> None:
        output = self.generate(
            """
            namespace tbx::tests
            {
            [[tbx::serializable]];
            [[tbx::version(4)]];
            struct Value
            {
                [[tbx::text]]
                std::string source = "";
                [[tbx::meta]]
                int import_version = 0;
            };
            }
            """
        )
        self.assertIn("register_asset_type<Value>(4)", output)
        self.assertIn("register_asset_body_type<Value>", output)
        self.assertIn("register_asset_meta_type<Value>", output)

    def test_custom_serializers_are_generated(self) -> None:
        output = self.generate(
            """
            namespace tbx::tests
            {
            [[tbx::serializable]];
            struct Value
            {
                int amount = 0;
            };

            [[tbx::serializable]];
            [[tbx::version(5)]];
            struct AssetValue : Asset
            {
                int amount = 0;
            };

            template <>
            struct Serializer<Value>
            {
            };

            template <>
            struct Serializer<AssetValue>
            {
            };
            }
            """
        )
        self.assertIn("Serializer<Value>::to_json", output)
        self.assertIn("read_custom_json_asset_body<AssetValue>", output)

    def test_enum_and_variant_are_generated(self) -> None:
        output = self.generate(
            """
            namespace tbx::tests
            {
            [[tbx::serializable]];
            enum class Mode
            {
                FULL [[tbx::name("full")]],
                LOW [[tbx::name("low")]]
            };

            [[tbx::serializable]];
            using Variant = std::variant<int, float>;
            }
            """
        )
        self.assertIn("NLOHMANN_JSON_SERIALIZE_ENUM(Mode", output)
        self.assertIn("::tbx::Json& json", output)
        self.assertIn("to_json_serializable_variant", output)

    def test_indexed_alias_is_generated(self) -> None:
        output = self.generate(
            """
            namespace glm
            {
            [[tbx::serializable]];
            [[tbx::name("Mat4")]];
            [[tbx::count(4U)]];
            using TbxMat4 = tbx::Mat4;
            }
            """
        )
        self.assertIn("namespace glm", output)
        self.assertIn("write_indexed_serialization_value", output)

    def test_printable_and_hash_are_generated(self) -> None:
        output = self.generate(
            """
            namespace tbx::tests
            {
            [[tbx::printable("Value: {}", value)]];
            [[tbx::hash(name, id)]];
            struct Value
            {
                int value = 0;
                std::string name = "";
                int id = 0;
            };
            }
            """
        )
        self.assertIn("struct std::formatter<tbx::tests::Value>", output)
        self.assertIn('std::format(\n                "Value: {}",', output)
        self.assertIn("struct std::hash<tbx::tests::Value>", output)
        self.assertIn("seed = ::tbx::hash_combine(seed, value.name);", output)
        self.assertIn("seed = ::tbx::hash_combine(seed, value.id);", output)

    def test_printable_and_hash_can_use_value_expression(self) -> None:
        output = self.generate(
            """
            namespace tbx::tests
            {
            [[tbx::printable("{}", tbx::to_string($))]];
            [[tbx::hash(tbx::hash($))]];
            struct Value
            {
                int value = 0;
            };
            }
            """
        )
        self.assertIn("tbx::to_string(value)", output)
        self.assertIn("return static_cast<::size>(tbx::hash(value));", output)

    def test_unscoped_enum_printable_is_generated(self) -> None:
        output = self.generate(
            """
            namespace tbx::tests
            {
            [[tbx::printable]];
            enum Unit
            {
                ONE [[tbx::name("one")]],
                TWO [[tbx::name("two")]]
            };
            }
            """
        )
        self.assertIn("struct std::formatter<tbx::tests::Unit>", output)
        self.assertIn("case tbx::tests::Unit::ONE:", output)
        self.assertIn('name = "one";', output)

    def test_asset_requires_version(self) -> None:
        with self.assertRaises(CodegenError):
            self.generate(
                """
                namespace tbx::tests
                {
                [[tbx::serializable]];
                struct Value : Asset
                {
                    [[tbx::prop]]
                    int amount = 0;
                };
                }
                """
            )
