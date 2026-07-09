#include "tbx/systems/assets/serialization.h"
#include "tbx/systems/ecs/entity.h"
#include "tbx/systems/ecs/entity_serialization.h"
#include "tbx/systems/ecs/registry.h"
#include "tbx/types/components/script_container.h"
#include "tbx/types/components/transform.h"
#include <string>

namespace
{
    // The Transform component is registered by the engine at static-init (its serializable registration
    // carries the describe thunk), so its schema is available without per-test codegen. Its wire
    // name is the snake-cased type name.
    constexpr std::string_view TRANSFORM_WIRE_NAME = "transform";

    // The component's attribute-enriched schema: a default-constructed instance serialized with attribute
    // metadata, exactly what the world.describe envelope carries per component. Null when the type is not
    // registered.
    tbx::Json describe_component(std::string_view wire_name)
    {
        for (const auto& registration : tbx::get_entity_component_type_registrations())
            if (registration.name == wire_name && registration.describe)
                return tbx::Json::parse(registration.describe(/*include_attributes=*/true), nullptr, false);
        return tbx::Json();
    }

    TEST(ReflectionTests, RegistersComponentSchemaByName)
    {
        const auto schema = describe_component(TRANSFORM_WIRE_NAME);
        ASSERT_TRUE(schema.is_object());
        EXPECT_TRUE(schema.contains("position"));
    }

    TEST(ReflectionTests, ExposesTypeMetadataOnProperties)
    {
        const auto schema = describe_component(TRANSFORM_WIRE_NAME);
        ASSERT_TRUE(schema.is_object());
        const auto& position = schema.at("position").at("attributes");
        EXPECT_EQ(position.at("type"), "vec3");
        EXPECT_TRUE(position.contains("order"));
    }

    TEST(ReflectionTests, UnknownTypeHasNoSchema)
    {
        EXPECT_TRUE(describe_component("not_a_real_type").is_null());
    }

    TEST(ReflectionTests, ComponentPropertyGetSetResetRoundtrip)
    {
        auto registry = tbx::EntityRegistry();
        const auto id = registry.add("probe");
        registry.add<tbx::Transform>(id);
        const auto entity = registry.get(id);

        // A fresh transform's position holds its default.
        auto is_default = false;
        ASSERT_TRUE(
            tbx::is_component_property_default(entity, "transform", "position", is_default)
                .succeeded());
        EXPECT_TRUE(is_default);

        // Set it through the round-trip path (no per-property accessor involved).
        ASSERT_TRUE(
            tbx::apply_component_property(entity, "transform", "position", "[1.0, 2.0, 3.0]")
                .succeeded());

        auto node = std::string();
        ASSERT_TRUE(
            tbx::serialize_component_property(entity, "transform", "position", node).succeeded());
        const auto parsed = tbx::Json::parse(node);
        EXPECT_EQ(parsed.at("type"), "vec3");
        EXPECT_EQ(parsed.at("value"), tbx::Json::array({ 1.0, 2.0, 3.0 }));

        tbx::is_component_property_default(entity, "transform", "position", is_default);
        EXPECT_FALSE(is_default);

        // Reset returns it to the captured default.
        ASSERT_TRUE(
            tbx::reset_component_property(entity, "transform", "position").succeeded());
        tbx::is_component_property_default(entity, "transform", "position", is_default);
        EXPECT_TRUE(is_default);
    }

    TEST(ReflectionTests, SerializedFieldsCarryNoAttributeMetadata)
    {
        // A direct serialize includes every field (no omit scope) and stays lean { "type", "value" }
        // with no attributes wrapper — schema metadata travels only on the attribute path.
        const auto transform = tbx::Transform {};
        auto json = tbx::Json();
        tbx::serialize(json, transform);

        ASSERT_TRUE(json.contains("position"));
        const auto& position = json.at("position");
        EXPECT_TRUE(position.contains("type"));
        EXPECT_TRUE(position.contains("value"));
        EXPECT_FALSE(position.contains("attributes"));
        EXPECT_FALSE(position.contains("order"));
    }

    TEST(ReflectionTests, AttributeSerializeReinjectsMetadataThatLeanFormOmits)
    {
        auto registry = tbx::EntityRegistry();
        const auto id = registry.add("probe");
        registry.add<tbx::Transform>(id);
        const auto entity = registry.get(id);
        // Give position a non-default value so the lean form keeps it (defaults are omitted on write).
        ASSERT_TRUE(
            tbx::apply_component_property(entity, "transform", "position", "[1.0, 2.0, 3.0]")
                .succeeded());

        const auto lean = tbx::Json::parse(tbx::Entity::serialize(entity));
        const auto attributed = tbx::Json::parse(
            tbx::Entity::serialize(entity, /*include_defaults=*/true, /*include_attributes=*/true));

        // The component is keyed by its snake-cased wire name.
        const auto& lean_position = lean.at("components").at("transform").at("position");
        const auto& attributed_position =
            attributed.at("components").at("transform").at("position");

        // The lean form stays { "type", "value" } with no metadata and no attributes wrapper.
        EXPECT_TRUE(lean_position.contains("type"));
        EXPECT_TRUE(lean_position.contains("value"));
        EXPECT_FALSE(lean_position.contains("attributes"));

        // Attribute enrichment reshapes each node to { "attributes": { "type", <metadata> }, "value" }.
        EXPECT_TRUE(attributed_position.contains("value"));
        EXPECT_EQ(attributed_position.at("attributes").at("type"), "vec3");
        EXPECT_TRUE(attributed_position.at("attributes").contains("order"));

        // The entity id is enriched by the attribute form but stays lean otherwise.
        EXPECT_FALSE(lean.at("id").contains("attributes"));
        EXPECT_TRUE(attributed.at("id").contains("attributes"));
    }

    TEST(ReflectionTests, SerializeOmitsDefaultsWhileAttributeFormIncludesAndFlagsThem)
    {
        auto registry = tbx::EntityRegistry();
        const auto id = registry.add("probe");
        registry.add<tbx::Transform>(id);
        const auto entity = registry.get(id);

        // A fresh transform's position is at its default, so the lean (persisted) form omits it, while the
        // engine-assigned id is always written.
        {
            const auto lean = tbx::Json::parse(tbx::Entity::serialize(entity));
            const auto& component = lean.at("components").at("transform");
            EXPECT_TRUE(component.contains("id"));
            EXPECT_FALSE(component.contains("position"));
        }

        // include_defaults == true writes every field, default or not.
        {
            const auto full = tbx::Json::parse(tbx::Entity::serialize(entity, /*include_defaults=*/true));
            EXPECT_TRUE(full.at("components").at("transform").contains("position"));
        }

        // The attribute form always includes every field and flags whether each holds its default.
        {
            const auto attributed = tbx::Json::parse(
                tbx::Entity::serialize(entity, /*include_defaults=*/true, /*include_attributes=*/true));
            const auto& position = attributed.at("components").at("transform").at("position");
            EXPECT_TRUE(position.contains("value"));
            EXPECT_TRUE(position.value("is_default", false));
        }

        // Once modified, the property is persisted by the lean form and no longer flagged as default.
        ASSERT_TRUE(
            tbx::apply_component_property(entity, "transform", "position", "[1.0, 2.0, 3.0]")
                .succeeded());
        {
            const auto lean = tbx::Json::parse(tbx::Entity::serialize(entity));
            EXPECT_TRUE(lean.at("components").at("transform").contains("position"));
            const auto attributed = tbx::Json::parse(
                tbx::Entity::serialize(entity, /*include_defaults=*/true, /*include_attributes=*/true));
            EXPECT_FALSE(
                attributed.at("components").at("transform").at("position").value("is_default", true));
        }
    }

    TEST(ReflectionTests, VectorPropertyAdvertisesElementTemplate)
    {
        // script_container.scripts is a std::vector, so its attribute node carries an element_template the
        // editor clones to append a new entry — even to an empty list (this default container has none).
        const auto schema = describe_component("script_container");
        ASSERT_TRUE(schema.is_object());
        const auto& scripts = schema.at("scripts").at("attributes");
        EXPECT_EQ(scripts.at("type"), "array");
        ASSERT_TRUE(scripts.contains("element_template"));
        // The template is one default element, attribute-enriched exactly like the real elements (here a
        // binding whose script reference is a handle), so it can be appended verbatim.
        EXPECT_TRUE(scripts.at("element_template").contains("script"));
    }

    TEST(ReflectionTests, AttributeSerializeCarriesDeclarationOrderAndComponentOrder)
    {
        auto registry = tbx::EntityRegistry();
        const auto id = registry.add("probe");
        registry.add<tbx::Transform>(id);
        const auto entity = registry.get(id);

        const auto attributed = tbx::Json::parse(
            tbx::Entity::serialize(entity, /*include_defaults=*/true, /*include_attributes=*/true));

        // Each property carries its declaration index so the editor can restore source order over the
        // alphabetical JSON keys.
        const auto& transform = attributed.at("components").at("transform");
        EXPECT_TRUE(transform.at("position").at("attributes").contains("order"));

        // The entity lists its components in registration order alongside the (alphabetical) map.
        ASSERT_TRUE(attributed.contains("component_order"));
        ASSERT_TRUE(attributed.at("component_order").is_array());
        EXPECT_EQ(attributed.at("component_order").at(0), "transform");
    }

    TEST(ReflectionTests, EntityEnvelopeRoundTripsOrder)
    {
        auto registry = tbx::EntityRegistry();
        const auto id = registry.add("ordered");
        auto entity = registry.get(id);
        entity.set_order(7);

        const auto serialized = tbx::Entity::serialize(entity, /*include_defaults=*/true);
        const auto parsed = tbx::Json::parse(serialized);
        ASSERT_TRUE(parsed.contains("order"));
        EXPECT_EQ(parsed.at("order").at("value"), 7);

        auto restored_registry = tbx::EntityRegistry();
        auto restored = tbx::Entity();
        ASSERT_TRUE(tbx::Entity::deserialize(serialized, restored_registry, restored));
        EXPECT_EQ(restored.get_order(), 7);
    }

    TEST(ReflectionTests, AttributeSerializeRecursesIntoNestedArrayProperties)
    {
        auto registry = tbx::EntityRegistry();
        const auto id = registry.add("probe");
        auto& container = registry.add<tbx::ScriptContainer>(id);
        auto binding = tbx::ScriptContainerBinding {};
        binding.script.id = tbx::Uuid { 4242U };
        container.scripts.push_back(binding);
        const auto entity = registry.get(id);

        const auto attributed = tbx::Json::parse(
            tbx::Entity::serialize(entity, /*include_defaults=*/true, /*include_attributes=*/true));
        const auto& component = attributed.at("components").at("script_container");

        // The component's own id carries its metadata under the attributes wrapper.
        EXPECT_TRUE(component.at("id").contains("attributes"));

        // The nested binding's script reference is a handle (asset picker), and its binding_id — a
        // nested id — is reshaped by the recursive enrichment. Nested struct fields are reshaped to
        // { "attributes", "value" } too; the array's elements live under the parent's "value".
        const auto& first_binding = component.at("scripts").at("value").at(0);
        EXPECT_EQ(first_binding.at("script").at("attributes").at("type"), "handle");
        EXPECT_TRUE(first_binding.at("binding_id").contains("attributes"));
    }
}
