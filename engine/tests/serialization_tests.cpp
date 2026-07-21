#include "tbx/systems/assets/serialization.h"
#include "tbx/systems/ecs/entity.h"
#include "tbx/systems/ecs/entity_serialization.h"
#include "tbx/systems/ecs/registry.h"
#include "tbx/types/components/script_container.h"
#include "tbx/types/components/transform.h"
#include <string>

namespace
{
    TEST(SerializationTests, FieldsSerializeAsPlainValues)
    {
        // A direct serialize writes each field as its bare value — no { "type", "value" } wrapper and
        // no attribute metadata.
        const auto transform = tbx::Transform {};
        auto json = tbx::Json();
        tbx::serialize(json, transform);

        ASSERT_TRUE(json.contains("position"));
        const auto& position = json.at("position");
        EXPECT_TRUE(position.is_array());
        EXPECT_FALSE(position.contains("type"));
        EXPECT_FALSE(position.contains("value"));
        EXPECT_FALSE(position.contains("attributes"));
    }

    TEST(SerializationTests, ComponentPropertyGetSetRoundtrip)
    {
        auto registry = tbx::EntityRegistry();
        const auto id = registry.add("probe");
        registry.add<tbx::Transform>(id);
        const auto entity = registry.get(id);

        // Set a property from its bare serialized value (no per-property accessor involved).
        ASSERT_TRUE(
            tbx::apply_component_property(entity, "transform", "position", "[1.0, 2.0, 3.0]")
                .succeeded());

        // Read it back as the same bare value.
        auto value = std::string();
        ASSERT_TRUE(
            tbx::serialize_component_property(entity, "transform", "position", value).succeeded());
        EXPECT_EQ(tbx::Json::parse(value), tbx::Json::array({ 1.0, 2.0, 3.0 }));
    }

    TEST(SerializationTests, SerializeOmitsDefaultsWhileIncludeDefaultsWritesThem)
    {
        auto registry = tbx::EntityRegistry();
        const auto id = registry.add("probe");
        registry.add<tbx::Transform>(id);
        const auto entity = registry.get(id);

        // A fresh transform's position is at its default, so the lean (persisted) form omits it, while
        // the engine-assigned id is always written (never equal to a default Uuid).
        {
            const auto lean = tbx::Json::parse(tbx::Entity::serialize(entity));
            const auto& component = lean.at("components").at("transform");
            EXPECT_TRUE(component.contains("id"));
            EXPECT_FALSE(component.contains("position"));
        }

        // include_defaults == true writes every field, default or not.
        {
            const auto full =
                tbx::Json::parse(tbx::Entity::serialize(entity, /*include_defaults=*/true));
            EXPECT_TRUE(full.at("components").at("transform").contains("position"));
        }

        // Once modified, the property is persisted by the lean form too.
        ASSERT_TRUE(
            tbx::apply_component_property(entity, "transform", "position", "[1.0, 2.0, 3.0]")
                .succeeded());
        {
            const auto lean = tbx::Json::parse(tbx::Entity::serialize(entity));
            EXPECT_TRUE(lean.at("components").at("transform").contains("position"));
        }
    }

    TEST(SerializationTests, EntityEnvelopeRoundTripsOrder)
    {
        auto registry = tbx::EntityRegistry();
        const auto id = registry.add("ordered");
        auto entity = registry.get(id);
        entity.set_order(7);

        const auto serialized = tbx::Entity::serialize(entity, /*include_defaults=*/true);
        const auto parsed = tbx::Json::parse(serialized);
        ASSERT_TRUE(parsed.contains("order"));
        EXPECT_EQ(parsed.at("order"), 7);

        auto restored_registry = tbx::EntityRegistry();
        auto restored = tbx::Entity();
        ASSERT_TRUE(tbx::Entity::deserialize(serialized, restored_registry, restored));
        EXPECT_EQ(restored.get_order(), 7);
    }

    TEST(SerializationTests, NestedArrayPropertyRoundTrips)
    {
        auto registry = tbx::EntityRegistry();
        const auto id = registry.add("probe");
        auto& container = registry.add<tbx::ScriptContainer>(id);
        auto binding = tbx::ScriptContainerBinding {};
        binding.script.id = tbx::Uuid { 4242U };
        container.scripts.push_back(binding);
        const auto entity = registry.get(id);

        const auto serialized = tbx::Entity::serialize(entity, /*include_defaults=*/true);
        const auto parsed = tbx::Json::parse(serialized);
        const auto& scripts = parsed.at("components").at("script_container").at("scripts");

        // The vector serializes as a plain JSON array of its bare elements.
        ASSERT_TRUE(scripts.is_array());
        ASSERT_EQ(scripts.size(), 1U);
        EXPECT_TRUE(scripts.at(0).contains("script"));
    }
}
