from __future__ import annotations

import textwrap
import tempfile
import unittest
from pathlib import Path

from generator import (
    GENERATED_CODE_BANNER,
    generate_header,
    generate_module_registration_header,
    generate_module_registration_source,
    generate_source,
    registrar_call_lines,
    run_module_registration_codegen,
)
from model import CodegenError, external_name, fields_of, find_attr
from parser import parse_source
from resource_codegen import generate_builtin_asset_headers, generate_material_instance_header


class AttributeCodegenTests(unittest.TestCase):
    def generate(self, source: str) -> str:
        return generate_header(parse_source(textwrap.dedent(source)))

    def generate_source(self, source: str) -> str:
        return generate_source(
            "value.generated.h",
            parse_source(textwrap.dedent(source)),
            "value.h",
        )

    def test_struct_fields_and_name_are_generated(self) -> None:
        source = """
            namespace tbx::tests
            {
            [[serializable]];
            [[name("renamed")]];
            struct Value
            {
                int amount = 0;
            };
            }
            """
        output = self.generate(source)
        source_output = self.generate_source(source)
        self.assertIn('return "renamed";', output)
        self.assertIn("void serialize(::tbx::Json& tbx_json, const Value& tbx_value);", output)
        self.assertIn(
            "void deserialize(const ::tbx::Json& tbx_json, Value& tbx_value);",
            output,
        )
        self.assertNotIn("tbx_value.amount", output)
        self.assertIn("::tbx::write_serialization_value<::tbx::Json>(", source_output)
        self.assertIn("tbx_value.amount);", source_output)
        self.assertTrue(output.startswith(GENERATED_CODE_BANNER))

    def test_editor_hint_attributes_are_passive(self) -> None:
        # icon/color/slider/hidden annotate a type/field for the editor property grid but must not
        # affect serialization: every field still serializes, and the hints ride along in the IR.
        source = """
            namespace tbx::tests
            {
            [[serializable]];
            [[icon(Move)]];
            [[color(Blue)]];
            struct Widget
            {
                [[slider(0, 1)]] float intensity = 0.0f;
                [[hidden]] int cache = 0;
                float normal_field = 0.0f;
            };
            }
            """
        widget = parse_source(textwrap.dedent(source))[0]
        self.assertIsNotNone(find_attr(widget.attrs, "icon"))
        self.assertIsNotNone(find_attr(widget.attrs, "color"))
        source_output = self.generate_source(source)
        for field_key in ('"intensity"', '"cache"', '"normal_field"'):
            self.assertIn(field_key, source_output)

    def test_editor_hint_attribute_arity_is_validated(self) -> None:
        source = """
            namespace tbx::tests
            {
            [[serializable]];
            struct Widget
            {
                [[slider(0, 1, 2)]] float intensity = 0.0f;
            };
            }
            """
        with self.assertRaises(CodegenError):
            self.generate(source)

    def test_single_field_struct_serialization_is_flattened(self) -> None:
        source = """
            namespace tbx::tests
            {
            [[serializable]];
            struct Value
            {
                int amount = 0;
            };
            }
            """

        output = self.generate_source(source)

        self.assertIn("tbx_json = ::tbx::write_serialization_value<::tbx::Json>(", output)
        self.assertIn("if (tbx_json.is_object() || tbx_json.is_null())", output)
        # The keyed branch reads the field back by key (the force_keyed asset-body form), while the
        # bare form reads the field value directly.
        self.assertIn("::tbx::read_serialization_field(", output)
        self.assertIn("::tbx::read_serialization_value(", output)

    def test_template_value_type_gets_header_only_passthrough_serialization(self) -> None:
        source = """
            namespace tbx
            {
            [[serializable]];
            template <typename TAsset>
                requires std::derived_from<TAsset, Asset>
            struct AssetHandle
            {
                Handle handle = {};
            };
            }
            """

        header = self.generate(source)
        source_output = self.generate_source(source)

        # The serialize/deserialize are function templates (parameterized on the wrapper's own
        # parameter) that pass straight through to the single field — byte-identical to serializing the
        # field on its own, so the object { "id": ... } form the field's deserializer accepts still works.
        self.assertIn("template <typename TAsset>", header)
        self.assertIn("void serialize(::tbx::Json& tbx_json, const AssetHandle<TAsset>& tbx_value)", header)
        self.assertIn("void deserialize(const ::tbx::Json& tbx_json, AssetHandle<TAsset>& tbx_value)", header)
        self.assertIn("::tbx::write_serialization_value<::tbx::Json>(", header)
        self.assertIn("tbx_value.handle);", header)
        self.assertIn("::tbx::read_serialization_value(", header)
        # A template is not one concrete type, so it carries none of the concrete-type glue: no runtime
        # registration, no keyed-object fallback, no serialization type name, and no forward declaration.
        self.assertNotIn("register_serializable_type", header)
        self.assertNotIn("TBX_SERIALIZATION_AUTO_REGISTER", source_output)
        self.assertNotIn("is_object()", header)
        self.assertNotIn("serialization_type_name", header)
        self.assertNotIn("struct AssetHandle;", header)

    def test_multi_field_struct_serialization_remains_object_shaped(self) -> None:
        source = """
            namespace tbx::tests
            {
            [[serializable]];
            struct Value
            {
                int amount = 0;

                int count = 0;
            };
            }
            """

        output = self.generate_source(source)

        self.assertIn("::tbx::write_serialization_field(", output)
        self.assertIn("::tbx::read_serialization_field(", output)
        self.assertIn('"amount"', output)
        self.assertIn('"count"', output)

    def test_no_reflection_registry_is_generated(self) -> None:
        source = """
            namespace tbx::tests
            {
            [[serializable]];
            struct Value
            {
                int amount = 0;

                int other = 0;
            };
            }
            """

        output = self.generate_source(source)

        # There is no reflection registry or metadata: no TypeReflection record, builder, registrar,
        # or per-field PropertyAttributeInfo descriptor — the generated serialize writes plain values.
        self.assertNotIn("TypeReflection", output)
        self.assertNotIn("register_type_reflection", output)
        self.assertNotIn("TBX_REFLECTION_AUTO_REGISTER", output)
        self.assertNotIn("tbx_build_type_reflection", output)
        self.assertNotIn("PropertyAttributeInfo", output)
        # The serializable registration is still emitted (name -> read/write) as an explicitly
        # callable registrar taking the per-module registration container.
        self.assertIn(
            "bool register_serializable_type(const Value*, ::tbx::RuntimeRegistrations* tbx_runtime)",
            output,
        )

    def test_multiline_field_initializer_is_parsed(self) -> None:
        source = """
            namespace tbx::tests
            {
            [[serializable]];
            struct Value
            {
                std::vector<int> values = {
                    1,
                    2,
                    3
                };
            };
            }
            """

        output = self.generate_source(source)

        self.assertIn("tbx_value.values", output)

    def test_generated_header_is_declaration_safe(self) -> None:
        output = self.generate(
            """
            namespace tbx::tests
            {
            [[serializable]];
            [[name("renamed")]];
            struct Value
            {
                int amount = 0;
            };
            }
            """
        )

        self.assertIn("std::true_type has_struct_serialization(const Value*);", output)
        self.assertIn(
            "bool register_serializable_type(const Value*, ::tbx::RuntimeRegistrations* tbx_runtime);",
            output,
        )
        self.assertNotIn("tbx_value.", output)
        self.assertNotIn("TBX_SERIALIZATION_AUTO_REGISTER", output)

    def test_source_stub_is_marked_generated(self) -> None:
        output = generate_source("value.generated.h", [], "value.h")

        self.assertTrue(output.startswith(GENERATED_CODE_BANNER))
        self.assertNotIn("#include \"value.h\"", output)
        self.assertNotIn("#include \"value.generated.h\"", output)

    def test_plugin_source_generates_fixed_exports(self) -> None:
        types = parse_source(
            textwrap.dedent(
                """
                namespace tbx::tests
                {
                [[register_plugin(
                    "ExamplePlugin",
                    "1.2.3",
                    tbx::PluginCategory::RENDERING,
                    25,
                    "WindowPlugin")]];
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

        self.assertIn("tbx_create_plugin", output)
        self.assertIn("tbx_destroy_plugin", output)
        self.assertIn("new tbx::tests::ExamplePlugin()", output)

    def test_plugin_source_supports_named_arguments(self) -> None:
        types = parse_source(
            textwrap.dedent(
                """
                namespace tbx::tests
                {
                [[register_plugin(
                    name = "ExamplePlugin",
                    version = "1.2.3",
                    category = tbx::PluginCategory::RENDERING,
                    priority = 25,
                    dependencies = {"WindowPlugin", "InputPlugin"})]];
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
        )

        self.assertIn('meta.name = "ExamplePlugin";', output)
        self.assertIn('meta.version = "1.2.3";', output)
        self.assertIn("meta.category = tbx::PluginCategory::RENDERING;", output)
        self.assertIn("meta.priority = 25U;", output)
        self.assertIn('meta.dependencies = {"WindowPlugin", "InputPlugin"};', output)

    def test_app_source_generates_app_exports(self) -> None:
        types = parse_source(
            textwrap.dedent(
                """
                namespace tbx::tests
                {
                [[app(name = "ExampleApp", version = "1.2.3")]];
                class ExampleApp final : public tbx::Application
                {
                };
                }
                """
            )
        )

        output = generate_source(
            "example_app.generated.h",
            types,
            "tbx/tests/example_app.h",
        )

        self.assertIn("#include \"tbx/systems/app/application.h\"", output)
        self.assertIn("TBX_APP_ENTRY_EXPORT ::tbx::Application* tbx_create_app()", output)
        self.assertIn("return new tbx::tests::ExampleApp();", output)
        self.assertIn(
            "TBX_APP_ENTRY_EXPORT void tbx_destroy_app(::tbx::Application* app)",
            output,
        )
        self.assertIn("delete app;", output)


    def test_named_attribute_arguments_are_generated(self) -> None:
        source = """
            namespace tbx::tests
            {
            [[serializable(mode = "json")]];
            [[name(value = "renamed")]];
            [[version(value = 3U)]];
            struct Value
            {
                int amount = 0;
            };
            }
            """

        header_output = self.generate(source)
        source_output = self.generate_source(source)

        self.assertIn('return "renamed";', header_output)
        self.assertIn("std::integral_constant<uint32, 3U>", header_output)
        self.assertIn("tbx_value.amount);", source_output)

    def test_parser_keeps_fields_as_neutral_metadata(self) -> None:
        source = """
            namespace tbx::tests
            {
            [[serializable]];
            struct Value
            {
                [[inject]]
                [[name("display_value")]]
                int amount = 0;
            };
            }
            """

        types = parse_source(textwrap.dedent(source))
        field = types[0].fields[0]

        self.assertEqual(field.access, "public")
        self.assertIsNotNone(find_attr(field.attrs, "inject"))
        self.assertEqual(external_name(field), "display_value")
        self.assertEqual(fields_of(types[0], "inject"), [field])

    def test_name_attribute_can_feed_independent_metadata_consumers(self) -> None:
        source = """
            namespace tbx::tests
            {
            [[serializable]];
            struct Value
            {
                [[name("display_value")]]
                int amount = 0;
            };
            }
            """

        field = parse_source(textwrap.dedent(source))[0].fields[0]

        serialization_key = external_name(field)
        editor_label = external_name(field)
        database_column = external_name(field)

        self.assertEqual(serialization_key, "display_value")
        self.assertEqual(editor_label, "display_value")
        self.assertEqual(database_column, "display_value")

    def test_gameplay_plugin_source_uses_default_dependencies(self) -> None:
        types = parse_source(
            textwrap.dedent(
                """
                namespace tbx::tests
                {
                [[register_plugin("ExamplePlugin", "1.2.3", tbx::PluginCategory::GAMEPLAY)]];
                class ExamplePlugin final : public tbx::Plugin
                {
                };
                }
                """
            )
        )

        output = generate_source("example_plugin.generated.h", types, "tbx/tests/example_plugin.h")

        self.assertIn(
            'meta.dependencies = {"JoltPhysics", "AssimpModelLoader", "StbImageLoader", '
            '"ShaderIncludeLoader", "PerformanceMonitor"};',
            output,
        )

    def test_plugin_source_generates_script_registration_method(self) -> None:
        plugin_types = parse_source(
            textwrap.dedent(
                """
                namespace tbx::tests
                {
                [[register_plugin("ExamplePlugin", "1.2.3")]];
                class ExamplePlugin final : public tbx::Plugin
                {
                };
                }
                """
            )
        )
        script_types = parse_source(
            textwrap.dedent(
                """
                namespace tbx::tests
                {
                [[register_script]];
                [[version(7U)]];
                class DoorController final : public tbx::Script
                {
                };
                }
                """
            )
        )

        output = generate_source(
            "example_plugin.generated.h",
            plugin_types,
            "tbx/tests/example_plugin.h",
            registration_types=script_types,
            registration_include_paths=["tbx/tests/door_controller.h"],
        )

        # The plugin source includes each script header and invokes its registration wrapper; the
        # script glue itself (register_script_type<...>, override/bind helpers) is defined in the
        # script's own generated source, not the plugin source.
        self.assertIn('#include "tbx/tests/door_controller.h"', output)
        self.assertIn("void tbx_register_plugin_services(", output)
        self.assertIn("tbx::tests::register_script_type_DoorController(*registrations);", output)
        self.assertNotIn("tbx_register_plugin_scripts", output)
        self.assertNotIn("tbx_unregister_plugin_scripts", output)
        self.assertNotIn("unregister_asset_type_entry", output)

        script_output = generate_source(
            "door_controller.generated.h",
            script_types,
            "tbx/tests/door_controller.h",
        )
        self.assertIn("register_script_type<", script_output)
        self.assertIn("apply_script_overrides_DoorController", script_output)
        self.assertIn("bind_script_runtime_DoorController", script_output)

    def test_plugin_source_generates_service_registration_method(self) -> None:
        types = parse_source(
            textwrap.dedent(
                """
                namespace tbx::tests
                {
                [[register_plugin("ExamplePlugin", "1.2.3")]];
                [[register(tbx::IWindowManager, create_window_manager)]];
                class ExamplePlugin final : public tbx::Plugin
                {
                  public:
                    std::shared_ptr<tbx::IWindowManager> create_window_manager(
                        tbx::ServiceProvider& service_provider);
                };
                }
                """
            )
        )

        output = generate_source("example_plugin.generated.h", types, "tbx/tests/example_plugin.h")

        self.assertIn("void tbx_register_plugin_services(", output)
        self.assertIn("void register_services(ExamplePlugin& tbx_value", output)
        self.assertIn("dynamic_cast<tbx::tests::ExamplePlugin*>", output)
        self.assertIn("tbx_value.create_window_manager(tbx_services)", output)
        self.assertIn(
            "tbx_services.register_service<tbx::IWindowManager>(std::move(tbx_service_0));",
            output,
        )
        self.assertIn("::tbx::register_runtime_services(*typed_plugin, *service_provider);", output)

    def test_plugin_source_supports_named_register_arguments(self) -> None:
        types = parse_source(
            textwrap.dedent(
                """
                namespace tbx::tests
                {
                [[register_plugin("ExamplePlugin", "1.2.3")]];
                [[register(service = tbx::IWindowManager, factory = create_window_manager)]];
                class ExamplePlugin final : public tbx::Plugin
                {
                  public:
                    std::shared_ptr<tbx::IWindowManager> create_window_manager(
                        tbx::ServiceProvider& service_provider);
                };
                }
                """
            )
        )

        output = generate_source("example_plugin.generated.h", types, "tbx/tests/example_plugin.h")

        self.assertIn("tbx_value.create_window_manager(tbx_services)", output)
        self.assertIn(
            "tbx_services.register_service<tbx::IWindowManager>(std::move(tbx_service_0));",
            output,
        )

    def test_plugin_source_generates_field_service_registration(self) -> None:
        types = parse_source(
            textwrap.dedent(
                """
                namespace tbx::tests
                {
                [[register_plugin("ExamplePlugin", "1.2.3")]];
                class ExamplePlugin final : public tbx::Plugin
                {
                  public:
                    [[register(tbx::IWindowManager)]]
                    std::shared_ptr<WindowManager> window_manager = {};
                };
                }
                """
            )
        )

        output = generate_source("example.generated.h", types, "example.h")

        self.assertIn("if (!tbx_value.window_manager)", output)
        self.assertIn(
            "tbx_value.window_manager = std::make_shared<WindowManager>();",
            output,
        )
        self.assertIn(
            "tbx_services.register_service<tbx::IWindowManager>(tbx_value.window_manager);",
            output,
        )
        self.assertIn("void tbx_register_plugin_services(", output)

    def test_field_service_registration_defaults_to_shared_ptr_value_type(self) -> None:
        output = self.generate_source(
            """
            namespace tbx::tests
            {
            struct RuntimeServices
            {
                [[register]]
                std::shared_ptr<WindowManager> window_manager = {};
            };
            }
            """
        )

        self.assertIn("tbx_value.window_manager = std::make_shared<WindowManager>();", output)
        self.assertIn(
            "tbx_services.register_service<WindowManager>(tbx_value.window_manager);",
            output,
        )

    def test_field_service_registration_supports_weak_ptr_observer(self) -> None:
        output = self.generate_source(
            """
            namespace tbx::tests
            {
            struct RuntimeServices
            {
                [[register]]
                std::weak_ptr<WindowManager> window_manager = {};
            };
            }
            """
        )

        self.assertIn("auto tbx_service_0 = tbx_value.window_manager.lock();", output)
        self.assertIn("tbx_service_0 = std::make_shared<WindowManager>();", output)
        self.assertIn("tbx_value.window_manager = tbx_service_0;", output)
        self.assertIn(
            "tbx_services.register_service<WindowManager>(tbx_service_0);",
            output,
        )

    def test_plugin_source_generates_inject_binding_method(self) -> None:
        types = parse_source(
            textwrap.dedent(
                """
                namespace tbx::tests
                {
                [[register_plugin("ExamplePlugin", "1.2.3")]];
                class ExamplePlugin final : public tbx::Plugin
                {
                  public:
                    [[inject]]
                    std::weak_ptr<tbx::IWindowManager> window_manager = {};
                };
                }
                """
            )
        )

        output = generate_source("example_plugin.generated.h", types, "tbx/tests/example_plugin.h")

        self.assertIn('#include "tbx/systems/scripting/service_ref.h"', output)
        self.assertIn("void tbx_bind_plugin_runtime(", output)
        self.assertIn("void bind_runtime(ExamplePlugin& tbx_value", output)
        self.assertIn("bind_service_field(tbx_value.window_manager", output)
        self.assertIn("::tbx::bind_runtime_fields(*typed_plugin, *service_provider);", output)

    def test_runtime_service_registration_is_generated_for_regular_types(self) -> None:
        source = """
            namespace tbx::tests
            {
            [[register(tbx::IWindowManager, create_window_manager)]];
            struct RuntimeServices
            {
                std::shared_ptr<tbx::IWindowManager> create_window_manager(
                    tbx::ServiceProvider& service_provider);
            };
            }
            """
        header_output = self.generate(source)
        source_output = self.generate_source(source)

        self.assertIn('#include "tbx/systems/scripting/service_ref.h"', header_output)
        self.assertIn(
            "void register_services(RuntimeServices& tbx_value, ::tbx::ServiceProvider& tbx_services);",
            header_output,
        )
        self.assertIn("void register_services(RuntimeServices& tbx_value", source_output)
        self.assertIn("tbx_value.create_window_manager(tbx_services)", source_output)
        self.assertIn(
            "tbx_services.register_service<tbx::IWindowManager>(std::move(tbx_service_0));",
            source_output,
        )

    def test_runtime_injection_is_generated_for_regular_types(self) -> None:
        source = """
            namespace tbx::tests
            {
            struct RuntimeConsumer
            {
                [[inject]]
                std::weak_ptr<tbx::IWindowManager> window_manager = {};
            };
            }
            """
        header_output = self.generate(source)
        source_output = self.generate_source(source)

        self.assertIn('#include "tbx/systems/scripting/service_ref.h"', header_output)
        self.assertIn(
            "void bind_runtime(RuntimeConsumer& tbx_value, ::tbx::ServiceProvider& tbx_services);",
            header_output,
        )
        self.assertIn("void bind_runtime(RuntimeConsumer& tbx_value", source_output)
        self.assertIn("::tbx::bind_service_field(tbx_value.window_manager, tbx_services);", source_output)

    def test_runtime_injection_accepts_weak_ptr(self) -> None:
        source = """
            namespace tbx::tests
            {
            struct RuntimeConsumer
            {
                [[inject]]
                std::weak_ptr<tbx::IWindowManager> window_manager = {};
            };
            }
            """
        header_output = self.generate(source)
        source_output = self.generate_source(source)

        self.assertIn(
            "void bind_runtime(RuntimeConsumer& tbx_value, ::tbx::ServiceProvider& tbx_services);",
            header_output,
        )
        self.assertIn("::tbx::bind_service_field(tbx_value.window_manager, tbx_services);", source_output)

    def test_runtime_injection_rejects_shared_ptr(self) -> None:
        source = """
            namespace tbx::tests
            {
            struct RuntimeConsumer
            {
                [[inject]]
                std::shared_ptr<tbx::IWindowManager> window_manager = {};
            };
            }
            """

        with self.assertRaisesRegex(
            CodegenError,
            r"\[\[tbx::inject\]\] cannot target std::shared_ptr<T>",
        ):
            self.generate(source)

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
        output = self.generate_source(
            """
            namespace tbx::tests
            {
            [[serializable]];
            [[version(2)]];
            struct Value : Asset
            {
                int amount = 0;
                [[meta]]
                int import_version = 0;
            };
            }
            """
        )
        self.assertIn("register_asset_body_type<Value>", output)
        self.assertIn("register_asset_meta_type<Value>", output)

    def test_no_semicolon_attribute_blocks_are_generated(self) -> None:
        source = """
            namespace tbx::tests
            {
            [[serializable]]
            [[name("renamed")]]
            struct Value
            {
                int amount = 0;
            };
            }
            """
        header_output = self.generate(source)
        source_output = self.generate_source(source)
        self.assertIn('return "renamed";', header_output)
        self.assertIn("tbx_value.amount);", source_output)

    def test_qualified_and_bare_attributes_are_equivalent(self) -> None:
        # Engine code (inside namespace tbx) writes bare attributes; examples and plugins write the
        # fully-qualified tbx:: form. Both normalize to the same metadata and generate identical glue.
        bare = """
            namespace tbx::tests
            {
            [[serializable]];
            [[name("renamed")]];
            struct Value
            {
                [[name("renamed_amount")]]
                int amount = 0;

                int count = 0;
            };
            }
            """
        qualified = """
            namespace tbx::tests
            {
            [[tbx::serializable]];
            [[tbx::name("renamed")]];
            struct Value
            {
                [[tbx::name("renamed_amount")]]
                int amount = 0;

                int count = 0;
            };
            }
            """
        self.assertEqual(self.generate_source(bare), self.generate_source(qualified))
        self.assertIn('"renamed_amount"', self.generate_source(qualified))

    def test_parent_field_props_are_generated(self) -> None:
        source = textwrap.dedent(
            """
            namespace tbx::tests
            {
            [[serializable]]
            struct Value : Base
            {
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
                int id = 0;
            };
            }
            """
        )

        output = generate_source(
            "value.generated.h",
            parse_source(source, context_source=context),
            "value.h",
        )

        self.assertIn("tbx_value.id,", output)
        self.assertIn("tbx_default_value.id);", output)
        self.assertIn("tbx_value.amount,", output)
        self.assertIn("tbx_default_value.amount);", output)

    def test_type_level_meta_is_rejected(self) -> None:
        with self.assertRaises(CodegenError):
            self.generate(
                """
                namespace tbx::tests
                {
                [[serializable]]
                [[version(1)]]
                [[meta(id)]]
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
            [[serializable]]
            struct Value : Base
            {
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
                int id = 0;
            };
            }
            """
        )

        output = generate_source(
            "value.generated.h",
            parse_source(source, context_source=context),
            "value.h",
        )

        self.assertIn("tbx_value.id,", output)
        self.assertIn("tbx_default_value.id);", output)
        self.assertIn("tbx_value.amount,", output)
        self.assertIn("tbx_default_value.amount);", output)

    def test_script_asset_registration_is_generated(self) -> None:
        output = self.generate_source(
            """
            namespace tbx::tests
            {
            [[register_script]]
            [[name("door_controller")]]
            [[version(1)]]
            class DoorController : public tbx::Script
            {
              public:
                float open_speed = 1.0F;

                [[inject]]
                std::weak_ptr<tbx::InputManager> input = {};
            };
            }
            """
        )
        self.assertIn("register_script_type<", output)
        self.assertIn("tbx_value.open_speed);", output)
        self.assertIn("bind_script_field(tbx_value.open_speed", output)
        self.assertIn("bind_script_field(tbx_value.input", output)
        self.assertNotIn("tbx_json[\"input\"]", output)
        # No reflection registry is emitted; the editor schema comes from the serializable registration.
        self.assertNotIn("TypeReflection", output)
        self.assertNotIn("register_type_reflection", output)

    def test_map_prop_serializes_through_generic_value_template(self) -> None:
        output = self.generate_source(
            """
            namespace tbx::tests
            {
            [[serializable]];
            struct Inner
            {
                int a = 0;
            };
            [[serializable]];
            struct Outer
            {
                std::map<Uuid, Inner> by_id = {};

                int count = 0;
            };
            }
            """
        )
        # Map fields serialize through the generic value template (no map-specific emission).
        self.assertIn("tbx_value.by_id,", output)

    def test_core_render_pipeline_script_registration_is_generated(self) -> None:
        output = self.generate_source(
            """
            namespace tbx
            {
            [[register_script]]
            [[name("ExtractFrame")]]
            [[version(1)]]
            class ExtractFrameRenderPipelineScript : public tbx::RenderPipelineScript
            {
            };
            }
            """
        )
        self.assertIn("register_script_type<", output)
        self.assertIn("tbx_json = ::tbx::Json::object();", output)

    def test_script_weak_ptr_props_generate_script_reference_glue(self) -> None:
        output = self.generate_source(
            """
            namespace tbx::tests
            {
            [[register_script]]
            [[version(1)]]
            class DoorController : public tbx::Script
            {
              public:
                std::weak_ptr<DoorController> linked_door = {};
            };
            }
            """
        )

        self.assertIn("write_script_reference_field(", output)
        self.assertIn("read_script_reference_field(", output)
        self.assertIn("parse_script_reference_value(", output)
        self.assertIn('set_script_reference("linked_door"', output)
        self.assertIn("bind_script_reference_field(", output)

    def test_version_only_asset_registers_type_only(self) -> None:
        output = self.generate_source(
            """
            namespace tbx::tests
            {
            [[serializable]];
            [[version(3)]];
            struct Value : Asset
            {
                [[do_not_serialize]]
                int amount = 0;
            };
            }
            """
        )
        self.assertIn("::tbx::register_asset_type<Value>(tbx_runtime, 3);", output)
        self.assertNotIn("register_asset_body_type<Value>", output)
        self.assertNotIn("register_asset_meta_type<Value>", output)

    def test_text_and_meta_asset_are_generated(self) -> None:
        output = self.generate_source(
            """
            namespace tbx::tests
            {
            [[serializable]];
            [[version(4)]];
            struct Value : Asset
            {
                [[text]]
                std::string source = "";
                [[meta]]
                int import_version = 0;
            };
            }
            """
        )
        self.assertIn("has_text_serialization", output)
        self.assertIn("register_asset_body_type<Value>", output)
        self.assertIn("register_asset_meta_type<Value>", output)

    def test_text_and_meta_non_asset_are_generated(self) -> None:
        output = self.generate_source(
            """
            namespace tbx::tests
            {
            [[serializable]];
            [[version(4)]];
            struct Value
            {
                [[text]]
                std::string source = "";
                [[meta]]
                int import_version = 0;
            };
            }
            """
        )
        self.assertIn("::tbx::register_asset_type<Value>(tbx_runtime, 4);", output)
        self.assertIn("register_asset_body_type<Value>", output)
        self.assertIn("register_asset_meta_type<Value>", output)

    def test_custom_serializers_are_generated(self) -> None:
        output = self.generate_source(
            """
            namespace tbx::tests
            {
            [[serializable]];
            struct Value
            {
                [[do_not_serialize]]
                int amount = 0;
            };

            [[serializable]];
            [[version(5)]];
            struct AssetValue : Asset
            {
                [[do_not_serialize]]
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
        self.assertIn("Serializer<Value>::serialize", output)
        self.assertIn("Serializer<AssetValue>::deserialize", output)
        self.assertIn("Serializer<AssetValue>::serialize", output)

    def test_custom_serialization_attribute_is_generated(self) -> None:
        output = self.generate_source(
            """
            namespace tbx::tests
            {
            [[serializable]];
            [[custom_serialization(
                ::tbx::Serializer<Value>::serialize,
                ::tbx::Serializer<Value>::deserialize)]];
            struct Value
            {
            };
            }
            """
        )
        self.assertIn(
            "tbx_json = ::tbx::Json::parse(::tbx::Serializer<Value>::serialize(tbx_value));",
            output,
        )
        self.assertIn(
            "if (!::tbx::Serializer<Value>::deserialize(tbx_serialization_data, tbx_serialization_value))",
            output,
        )

    def test_custom_serialization_attribute_supports_unqualified_method_names(self) -> None:
        output = self.generate_source(
            """
            namespace tbx::tests
            {
            [[serializable]];
            [[custom_serialization(serialize, deserialize)]];
            struct Value
            {
            };
            }
            """
        )
        self.assertIn("tbx_json = ::tbx::Json::parse(Value::serialize(tbx_value));", output)
        self.assertIn("if (!Value::deserialize(tbx_serialization_data, tbx_serialization_value))", output)

    def test_lifecycle_attributes_emit_hooks(self) -> None:
        output = self.generate_source(
            """
            namespace tbx::tests
            {
            [[serializable]];
            [[pre_serialize(prepare_for_save)]];
            [[post_deserialize(rebuild_runtime_state)]];
            struct Value
            {
                int amount = 0;
            };
            }
            """
        )
        self.assertIn("void pre_serialize(const Value& tbx_value)", output)
        self.assertIn("tbx_value.prepare_for_save();", output)
        self.assertIn("void post_deserialize(Value& tbx_value)", output)
        self.assertIn("tbx_value.rebuild_runtime_state();", output)

    def test_struct_serialization_glue_uses_concrete_json_overloads(self) -> None:
        output = self.generate(
            """
            namespace tbx::tests
            {
            [[serializable]];
            struct Value
            {
                int amount = 0;
            };
            }
            """
        )

        self.assertNotIn("template <typename BasicJsonType>", output)
        self.assertNotIn("template <typename TSerializable", output)
        self.assertNotIn("Serializer<TSerializable>", output)

    def test_enum_and_variant_are_generated(self) -> None:
        output = self.generate_source(
            """
            namespace tbx::tests
            {
            [[serializable]];
            enum class Mode
            {
                FULL [[name("full")]],
                LOW [[name("low")]]
            };

            [[serializable]];
            using Variant = std::variant<int, float>;
            }
            """
        )
        self.assertIn("void serialize(::tbx::Json& json, const Mode& value)", output)
        self.assertIn("::tbx::Json& json", output)
        self.assertIn("serialize_serializable_variant", output)

    def test_indexed_alias_is_generated(self) -> None:
        output = self.generate_source(
            """
            namespace glm
            {
            [[serializable]];
            [[name("Mat4")]];
            [[array(4U)]];
            using TbxMat4 = tbx::Mat4;
            }
            """
        )
        self.assertIn("namespace glm", output)
        self.assertIn("write_indexed_serialization_value", output)

    def test_printable_and_hash_are_generated(self) -> None:
        header_output = self.generate(
            """
            namespace tbx::tests
            {
            [[printable("Value: {}", value)]];
            [[hash(name, id)]];
            struct Value
            {
                int value = 0;
                std::string name = "";
                int id = 0;
            };
            }
            """
        )
        source_output = self.generate_source(
            """
            namespace tbx::tests
            {
            [[printable("Value: {}", value)]];
            [[hash(name, id)]];
            struct Value
            {
                int value = 0;
                std::string name = "";
                int id = 0;
            };
            }
            """
        )
        self.assertIn("struct std::formatter<tbx::tests::Value>", header_output)
        self.assertIn("struct std::hash<tbx::tests::Value>", header_output)
        self.assertIn("bool operator==(const Value& left, const Value& right);", header_output)
        self.assertIn('std::format(\n            "Value: {}",', source_output)
        self.assertIn("seed = ::tbx::hash_combine(seed, value.name);", source_output)
        self.assertIn("seed = ::tbx::hash_combine(seed, value.id);", source_output)
        self.assertIn(
            "bool operator==(const Value& left, const Value& right)",
            source_output,
        )
        self.assertIn("return ((left.name) == (right.name)) && ((left.id) == (right.id));", source_output)

    def test_hash_respects_existing_equality_operator(self) -> None:
        source = """
            namespace tbx::tests
            {
            [[hash(name, id)]]
            struct Value
            {
                std::string name = "";
                int id = 0;

                bool operator==(const Value& other) const = default;
            };
            }
            """

        header_output = self.generate(source)
        source_output = self.generate_source(source)

        self.assertIn("struct std::hash<tbx::tests::Value>", header_output)
        self.assertNotIn("bool operator==(const Value& left, const Value& right);", header_output)
        self.assertNotIn("bool operator==(const Value& left, const Value& right)", source_output)

    def test_hash_equality_uses_type_api_macro(self) -> None:
        output = self.generate(
            """
            namespace tbx::tests
            {
            [[hash(id)]]
            struct TBX_API Value
            {
                int id = 0;
            };
            }
            """
        )

        self.assertIn("TBX_API bool operator==(const Value& left, const Value& right);", output)

    def test_forward_declarations_are_generated_before_global_specializations(self) -> None:
        output = self.generate(
            """
            namespace tbx::tests
            {
            [[hash(name, id)]]
            struct Value
            {
                std::string name = "";
                int id = 0;
            };
            }
            """
        )

        declaration_index = output.find("struct Value;")
        specialization_index = output.find("struct std::hash<tbx::tests::Value>")

        self.assertNotEqual(-1, declaration_index)
        self.assertNotEqual(-1, specialization_index)
        self.assertLess(declaration_index, specialization_index)
        self.assertIn("::size operator()(const tbx::tests::Value& value) const", output)

    def test_scoped_enum_forward_declaration_preserves_underlying_type(self) -> None:
        output = self.generate(
            """
            namespace tbx::tests
            {
            [[printable]]
            enum class Mode : uint8
            {
                LOW [[name("low")]]
            };
            }
            """
        )

        self.assertIn("enum class Mode : uint8;", output)
        source_output = self.generate_source(
            """
            namespace tbx::tests
            {
            [[printable]]
            enum class Mode : uint8
            {
                LOW [[name("low")]]
            };
            }
            """
        )
        self.assertIn("case tbx::tests::Mode::LOW:", source_output)

    def test_alias_declaration_is_generated_before_alias_glue(self) -> None:
        output = self.generate(
            """
            namespace tbx::tests
            {
            [[serializable]]
            using Variant = std::variant<int, float>;
            }
            """
        )

        declaration_index = output.find("using Variant = std::variant<int, float>;")
        glue_index = output.find("void serialize(::tbx::Json& json, const Variant& value)")

        self.assertNotEqual(-1, declaration_index)
        self.assertNotEqual(-1, glue_index)
        self.assertLess(declaration_index, glue_index)

    def test_printable_and_hash_can_use_value_expression(self) -> None:
        output = self.generate_source(
            """
            namespace tbx::tests
            {
            [[printable("{}", tbx::to_string($))]];
            [[hash($.value)]];
            struct Value
            {
                int value = 0;
            };
            }
            """
        )
        self.assertIn("tbx::to_string(value)", output)
        self.assertIn("seed = ::tbx::hash_combine(seed, value.value);", output)

    def test_printable_positional_format_with_equals_uses_member_expressions(self) -> None:
        output = self.generate_source(
            """
            namespace tbx::tests
            {
            struct Meta
            {
                std::string name = "";
                std::string version = "";
            };

            [[printable("Name={}, Version={}", meta.name, meta.version)]];
            struct Value
            {
                Meta meta = {};
            };
            }
            """
        )

        self.assertIn('std::format(\n            "Name={}, Version={}",', output)
        self.assertIn("value.meta.name,\n            value.meta.version", output)

    def test_variant_hash_expands_value_expression(self) -> None:
        output = self.generate_source(
            """
            namespace tbx::tests
            {
            [[serializable]];
            [[hash($)]];
            using Variant = std::variant<int, float>;
            }
            """
        )

        self.assertIn("seed = ::tbx::hash_combine(seed, value.index());", output)
        self.assertIn("std::visit(", output)
        self.assertIn("seed = ::tbx::hash_combine(seed, tbx_variant_value);", output)

    def test_unscoped_enum_printable_is_generated(self) -> None:
        source = """
            namespace tbx::tests
            {
            [[printable]];
            enum Unit
            {
                ONE [[name("one")]],
                TWO [[name("two")]]
            };
            }
            """
        output = self.generate(source)
        source_output = self.generate_source(source)
        self.assertIn("struct std::formatter<tbx::tests::Unit>", output)
        self.assertIn("case tbx::tests::ONE:", source_output)
        self.assertIn('name = "one";', source_output)

    def test_asset_requires_version(self) -> None:
        with self.assertRaises(CodegenError):
            self.generate(
                """
                namespace tbx::tests
                {
                [[serializable]];
                struct Value : Asset
                {
                    [[do_not_serialize]]
                    int amount = 0;
                };
                }
                """
            )

    def test_public_members_serialize_by_default(self) -> None:
        output = self.generate_source(
            """
            namespace tbx::tests
            {
            [[serializable]];
            struct Value
            {
                int amount = 0;
                float ratio = 1.0F;
            };
            }
            """
        )
        self.assertIn('"amount"', output)
        self.assertIn('"ratio"', output)
        self.assertIn("tbx_value.amount", output)
        self.assertIn("tbx_value.ratio", output)

    def test_do_not_serialize_excludes_public_member(self) -> None:
        output = self.generate_source(
            """
            namespace tbx::tests
            {
            [[serializable]];
            struct Value
            {
                int amount = 0;
                [[do_not_serialize]]
                int runtime_only = 0;
            };
            }
            """
        )
        self.assertIn("tbx_value.amount", output)
        self.assertNotIn("runtime_only", output)

    def test_private_members_are_not_serialized_by_default(self) -> None:
        output = self.generate_source(
            """
            namespace tbx::tests
            {
            [[serializable]];
            class Value
            {
              public:
                int amount = 0;

              private:
                int _hidden = 0;
            };
            }
            """
        )
        self.assertIn("tbx_value.amount", output)
        self.assertNotIn("_hidden", output)

    def test_serialize_attribute_opts_in_private_member_via_access_broker(self) -> None:
        output = self.generate_source(
            """
            namespace tbx
            {
            [[serializable]];
            class Value
            {
              public:
                TBX_EXPOSE_PRIVATES_TO_SERIALIZATION;

              private:
                [[serialize]]
                int _amount = 0;
                int _runtime = 0;
            };
            }
            """
        )
        # The bodies live in a SerializationAccess<Value> specialization, and the free serialize /
        # deserialize functions delegate to it.
        self.assertIn("struct SerializationAccess<Value>", output)
        self.assertIn("::tbx::SerializationAccess<Value>::serialize(tbx_json, tbx_value);", output)
        self.assertIn("::tbx::SerializationAccess<Value>::deserialize(tbx_json, tbx_value);", output)
        self.assertIn("tbx_value._amount", output)
        self.assertNotIn("tbx_value._runtime", output)

    def test_non_public_serialize_outside_tbx_namespace_is_rejected(self) -> None:
        with self.assertRaises(CodegenError):
            self.generate_source(
                """
                namespace other
                {
                [[tbx::serializable]];
                class Value
                {
                  public:
                    TBX_EXPOSE_PRIVATES_TO_SERIALIZATION;

                  private:
                    [[tbx::serialize]]
                    int _amount = 0;
                };
                }
                """
            )

    def test_parser_skips_non_data_member_declarations(self) -> None:
        source = """
            namespace tbx::tests
            {
            [[serializable]];
            struct Value
            {
                using alias = int;
                static const Value DEFAULT;
                Value() = default;
                ~Value() = default;
                Value(int seed) : amount(seed) {}
                bool operator==(const Value& other) const { return amount == other.amount; }
                float compute() const
                {
                    auto scratch = 0.0F;
                    return scratch + static_cast<float>(amount);
                }
                template <typename T>
                T cast() const;

                int amount = 0;
                std::vector<int> values = { 1, 2, 3 };
            };
            }
            """
        types = parse_source(textwrap.dedent(source))
        field_names = [field.name for field in types[0].fields]
        self.assertEqual(field_names, ["amount", "values"])

    def test_register_script_requires_script_base(self) -> None:
        with self.assertRaises(CodegenError):
            self.generate_source(
                """
                namespace tbx::tests
                {
                [[register_script]]
                [[version(1)]]
                class NotAScript
                {
                };
                }
                """
            )

    def test_serializable_registrar_takes_runtime_registrations(self) -> None:
        source = """
            namespace tbx::tests
            {
            [[serializable]];
            struct Value
            {
                int amount = 0;
            };
            }
            """

        header_output = self.generate(source)
        source_output = self.generate_source(source)

        self.assertIn(
            "bool register_serializable_type(const Value*, ::tbx::RuntimeRegistrations* tbx_runtime);",
            header_output,
        )
        self.assertIn(
            "bool register_serializable_type(const Value*, ::tbx::RuntimeRegistrations* tbx_runtime)",
            source_output,
        )
        # The registrar forwards the runtime by reference into the engine template overload.
        self.assertIn("*tbx_runtime,", source_output)
        self.assertNotIn("TBX_SERIALIZATION_AUTO_REGISTER", source_output)

    def test_asset_registrations_are_aggregated_into_one_function(self) -> None:
        source = """
            namespace tbx::tests
            {
            [[serializable]];
            [[version(2)]];
            struct Value : Asset
            {
                int amount = 0;
                [[meta]]
                int import_version = 0;
            };
            }
            """

        header_output = self.generate(source)
        source_output = self.generate_source(source)

        self.assertIn(
            "bool register_asset_registrations_Value(::tbx::RuntimeRegistrations& tbx_runtime);",
            header_output,
        )
        self.assertIn(
            "bool register_asset_registrations_Value(::tbx::RuntimeRegistrations& tbx_runtime)",
            source_output,
        )
        type_index = source_output.find("::tbx::register_asset_type<Value>(tbx_runtime, 2);")
        body_index = source_output.find("::tbx::register_asset_body_type<Value>(")
        meta_index = source_output.find("::tbx::register_asset_meta_type<Value>(")
        function_index = source_output.find("bool register_asset_registrations_Value(")
        self.assertNotEqual(-1, type_index)
        self.assertNotEqual(-1, body_index)
        self.assertNotEqual(-1, meta_index)
        self.assertLess(function_index, type_index)
        self.assertLess(type_index, body_index)
        self.assertLess(body_index, meta_index)
        self.assertIn("tbx_runtime,", source_output)
        self.assertNotIn("TBX_SERIALIZATION_AUTO_REGISTER", source_output)

    def test_script_registrar_takes_runtime_registrations(self) -> None:
        source = """
            namespace tbx::tests
            {
            [[register_script]]
            [[version(1)]]
            class DoorController : public tbx::Script
            {
              public:
                float open_speed = 1.0F;
            };
            }
            """

        header_output = self.generate(source)
        source_output = self.generate_source(source)

        self.assertIn(
            "bool register_script_type_DoorController(::tbx::RuntimeRegistrations& tbx_runtime);",
            header_output,
        )
        self.assertIn(
            "bool register_script_type_DoorController(::tbx::RuntimeRegistrations& tbx_runtime)",
            source_output,
        )
        self.assertIn(">(tbx_runtime, 1);", source_output)
        self.assertNotIn("TBX_SERIALIZATION_AUTO_REGISTER", source_output)

    def test_plugin_entries_take_registrations_and_call_registrars(self) -> None:
        plugin_types = parse_source(
            textwrap.dedent(
                """
                namespace tbx::tests
                {
                [[register_plugin("ExamplePlugin", "1.2.3")]];
                [[register(tbx::IWindowManager, create_window_manager)]];
                class ExamplePlugin final : public tbx::Plugin
                {
                  public:
                    std::shared_ptr<tbx::IWindowManager> create_window_manager(
                        tbx::ServiceProvider& service_provider);

                    [[inject]]
                    std::weak_ptr<tbx::IWindowManager> window_manager = {};
                };
                }
                """
            )
        )
        registration_types = parse_source(
            textwrap.dedent(
                """
                namespace tbx::tests
                {
                [[serializable]];
                struct Settings
                {
                    int amount = 0;

                    int count = 0;
                };

                [[serializable]];
                [[version(2)]];
                struct Level : Asset
                {
                    int seed = 0;
                };

                [[register_script]];
                [[version(7U)]];
                class DoorController final : public tbx::Script
                {
                };
                }
                """
            )
        )

        output = generate_source(
            "example_plugin.generated.h",
            plugin_types,
            "tbx/tests/example_plugin.h",
            registration_types=registration_types,
            registration_include_paths=["tbx/tests/game_types.h"],
        )

        self.assertIn('#include "tbx/tests/game_types.h"', output)
        self.assertIn(
            "TBX_PLUGIN_ENTRY_EXPORT void tbx_register_plugin_services(\n"
            "    ::tbx::Plugin* plugin,\n"
            "    ::tbx::ServiceProvider* service_provider,\n"
            "    ::tbx::RuntimeRegistrations* registrations)",
            output,
        )
        self.assertIn(
            "TBX_PLUGIN_ENTRY_EXPORT void tbx_bind_plugin_runtime(\n"
            "    ::tbx::Plugin* plugin,\n"
            "    ::tbx::ServiceProvider* service_provider,\n"
            "    ::tbx::RuntimeRegistrations* registrations)",
            output,
        )
        self.assertIn(
            "if (plugin == nullptr || service_provider == nullptr || registrations == nullptr)",
            output,
        )
        self.assertIn("tbx::tests::register_serializable_type(", output)
        self.assertIn("static_cast<const tbx::tests::Settings*>(nullptr),", output)
        self.assertIn("tbx::tests::register_asset_registrations_Level(*registrations);", output)
        self.assertIn("tbx::tests::register_script_type_DoorController(*registrations);", output)
        # Type registrars run before service registration.
        registrar_index = output.find("tbx::tests::register_serializable_type(")
        services_index = output.find("::tbx::register_runtime_services(")
        self.assertNotEqual(-1, services_index)
        self.assertLess(registrar_index, services_index)

    def test_app_source_generates_type_registration_export(self) -> None:
        app_types = parse_source(
            textwrap.dedent(
                """
                namespace tbx::tests
                {
                [[app(name = "ExampleApp", version = "1.2.3")]];
                class ExampleApp final : public tbx::Application
                {
                };
                }
                """
            )
        )
        registration_types = parse_source(
            textwrap.dedent(
                """
                namespace tbx::tests
                {
                [[serializable]];
                struct Settings
                {
                    int amount = 0;

                    int count = 0;
                };
                }
                """
            )
        )

        output = generate_source(
            "example_app.generated.h",
            app_types,
            "tbx/tests/example_app.h",
            registration_types=registration_types,
            registration_include_paths=["tbx/tests/game_types.h"],
        )

        self.assertIn('#include "tbx/tests/game_types.h"', output)
        self.assertIn(
            "TBX_APP_ENTRY_EXPORT void tbx_register_app_types(::tbx::RuntimeRegistrations* registrations)",
            output,
        )
        self.assertIn("if (registrations == nullptr)", output)
        self.assertIn("tbx::tests::register_serializable_type(", output)
        self.assertIn("static_cast<const tbx::tests::Settings*>(nullptr),", output)

        # The export is emitted even when the app module contributes no registrars.
        empty_output = generate_source(
            "example_app.generated.h",
            app_types,
            "tbx/tests/example_app.h",
        )
        self.assertIn(
            "TBX_APP_ENTRY_EXPORT void tbx_register_app_types(::tbx::RuntimeRegistrations* registrations)",
            empty_output,
        )
        self.assertNotIn("register_serializable_type(", empty_output)

    def test_module_registration_mode_emits_header_and_source(self) -> None:
        module_types = parse_source(
            textwrap.dedent(
                """
                namespace tbx::tests
                {
                [[serializable]];
                struct Settings
                {
                    int amount = 0;

                    int count = 0;
                };

                [[serializable]];
                [[version(2)]];
                struct Level : Asset
                {
                    int seed = 0;
                };

                [[serializable]];
                enum class Mode
                {
                    FULL [[name("full")]]
                };
                }
                """
            )
        )

        call_lines: list[str] = []
        for module_type in module_types:
            call_lines.extend(registrar_call_lines(module_type, "registrations", "&registrations"))
        header_output = generate_module_registration_header("engine", "TBX_API")
        source_output = generate_module_registration_source(
            "engine",
            "engine_types.generated.h",
            [("tbx/tests/game_types.h", "game_types.generated.h", call_lines)],
        )

        self.assertIn("#pragma once", header_output)
        self.assertIn('#include "tbx/tbx_api.h"', header_output)
        self.assertIn("class RuntimeRegistrations;", header_output)
        self.assertIn(
            "TBX_API void register_engine_types(RuntimeRegistrations& registrations);",
            header_output,
        )
        self.assertIn('#include "engine_types.generated.h"', source_output)
        self.assertIn('#include "tbx/tests/game_types.h"', source_output)
        self.assertIn('#include "game_types.generated.h"', source_output)
        self.assertIn(
            "void register_engine_types(RuntimeRegistrations& registrations)",
            source_output,
        )
        self.assertIn("tbx::tests::register_serializable_type(", source_output)
        self.assertIn("static_cast<const tbx::tests::Settings*>(nullptr),", source_output)
        self.assertIn("&registrations);", source_output)
        self.assertIn("tbx::tests::register_asset_registrations_Level(registrations);", source_output)
        # A plain enum has no runtime registrar; nothing about it lands in the aggregator.
        self.assertNotIn("Mode", source_output)

    def test_module_registration_codegen_resolves_nested_generated_includes(self) -> None:
        # The aggregator cpp must include each input's generated header at the SAME relative path as
        # the input (the generated tree mirrors the source layout); a bare stem would not resolve.
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            nested_dir = root / "include" / "tbx" / "types"
            nested_dir.mkdir(parents=True)
            (nested_dir / "widget.h").write_text(
                textwrap.dedent(
                    """
                    namespace tbx::tests
                    {
                    [[serializable]];
                    struct Widget
                    {
                        int amount = 0;

                        int count = 0;
                    };
                    }
                    """
                ),
                encoding="utf-8",
            )

            output_stem = root / "generated" / "engine_types"
            run_module_registration_codegen(
                "engine",
                "TBX_API",
                output_stem,
                [nested_dir / "widget.h"],
                root / "include",
            )

            source_output = (root / "generated" / "engine_types.generated.cpp").read_text(
                encoding="utf-8"
            )
            self.assertIn('#include "tbx/types/widget.h"', source_output)
            self.assertIn('#include "tbx/types/widget.generated.h"', source_output)

    def test_types_without_registrars_contribute_no_registrar_calls(self) -> None:
        module_types = parse_source(
            textwrap.dedent(
                """
                namespace tbx
                {
                [[serializable]];
                template <typename TAsset>
                struct AssetHandle
                {
                    Handle handle = {};
                };

                [[serializable]];
                using Variant = std::variant<int, float>;
                }
                """
            )
        )

        for module_type in module_types:
            self.assertEqual(
                [],
                registrar_call_lines(module_type, "registrations", "&registrations"),
            )
