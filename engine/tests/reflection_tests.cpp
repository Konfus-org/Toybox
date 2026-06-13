#include "tbx/systems/assets/serialization.h"
#include "tbx/systems/ecs/entity_registry.h"
#include "tbx/systems/reflection/reflection.h"
#include "tbx/types/components/script_container.h"
#include "tbx/types/components/transform.h"
#include <string>

namespace
{
    // The Transform component is registered by the engine, so its reflection record is available without
    // any per-test codegen. Its wire name is the snake-cased type name.
    constexpr std::string_view TRANSFORM_WIRE_NAME = "transform";

    TEST(ReflectionTests, ReflectsRegisteredComponentByName)
    {
        const auto info = tbx::reflect(TRANSFORM_WIRE_NAME);
        ASSERT_TRUE(info.valid());
        EXPECT_EQ(info.type_name(), "Transform");
        EXPECT_EQ(info.name(), TRANSFORM_WIRE_NAME);
        EXPECT_FALSE(info.properties().empty());
        EXPECT_TRUE(info.methods().empty());
    }

    TEST(ReflectionTests, ReflectsTypeIcon)
    {
        const auto info = tbx::reflect<tbx::Transform>();
        ASSERT_TRUE(info.valid());
        EXPECT_EQ(info.icon().name, "Move3d");
        EXPECT_EQ(info.icon().color, "BLUE");
    }

    TEST(ReflectionTests, ExposesEditorAttributesOnProperties)
    {
        const auto info = tbx::reflect<tbx::Transform>();
        const auto position = info.property("position");
        ASSERT_TRUE(position.valid());
        ASSERT_NE(position.descriptor(), nullptr);
        EXPECT_EQ(position.descriptor()->category, "Transform");
        EXPECT_EQ(position.descriptor()->description, "Local-space position, in metres.");
        EXPECT_EQ(position.type_token(), "vec3");
    }

    TEST(ReflectionTests, GetSetAndIsDefaultRoundtrip)
    {
        auto transform = tbx::Transform {};
        auto info = tbx::reflect(transform);
        ASSERT_TRUE(info.valid());
        ASSERT_TRUE(info.has_instance());

        // position has a deterministic default ([0,0,0]); the inherited id is engine-generated and so is
        // never "default" by value, which is why whole-object is_default() is not asserted true here.
        const auto position = info.property("position");
        ASSERT_TRUE(position.valid());
        EXPECT_TRUE(position.is_default());

        const auto new_value = tbx::Json::array({ 1.0, 2.0, 3.0 });
        EXPECT_TRUE(position.set(new_value));
        EXPECT_EQ(position.get(), new_value);
        EXPECT_EQ(transform.position, tbx::Vec3(1.0F, 2.0F, 3.0F));
        EXPECT_FALSE(position.is_default());

        // Mutating a property must drop the whole-object default check too.
        EXPECT_FALSE(info.is_default());
    }

    TEST(ReflectionTests, ConstInstanceRefusesSet)
    {
        const auto transform = tbx::Transform {};
        const auto info = tbx::reflect(transform);
        const auto position = info.property("position");
        ASSERT_TRUE(position.valid());
        EXPECT_FALSE(position.set(tbx::Json::array({ 9.0, 9.0, 9.0 })));
    }

    TEST(ReflectionTests, TypeOnlyHandleHasNoInstance)
    {
        const auto info = tbx::reflect(TRANSFORM_WIRE_NAME);
        ASSERT_TRUE(info.valid());
        EXPECT_FALSE(info.has_instance());
        EXPECT_FALSE(info.is_default());
        EXPECT_TRUE(info.get_value("position").is_null());
        EXPECT_FALSE(info.set_value("position", tbx::Json::array({ 1.0, 2.0, 3.0 })));
    }

    TEST(ReflectionTests, UnknownTypeYieldsInvalidHandle)
    {
        const auto info = tbx::reflect(std::string_view("not_a_real_type"));
        EXPECT_FALSE(info.valid());
        EXPECT_TRUE(info.properties().empty());
    }

    TEST(ReflectionTests, SerializedFieldsCarryNoEditorMetadata)
    {
        // Serialization stays lean { "type", "value" }; editor metadata lives only in reflection.
        const auto transform = tbx::Transform {};
        auto json = tbx::Json();
        tbx::serialize(json, transform);

        ASSERT_TRUE(json.contains("position"));
        const auto& position = json.at("position");
        EXPECT_TRUE(position.contains("type"));
        EXPECT_TRUE(position.contains("value"));
        EXPECT_FALSE(position.contains("category"));
        EXPECT_FALSE(position.contains("description"));
        EXPECT_FALSE(position.contains("readonly"));
        EXPECT_FALSE(position.contains("icon"));
    }

    TEST(ReflectionTests, DescribeReinjectsEditorMetadataThatSerializeOmits)
    {
        auto registry = tbx::EntityRegistry();
        const auto id = registry.add("probe");
        registry.add<tbx::Transform>(id);
        const auto entity = registry.get(id);

        const auto lean = tbx::Json::parse(tbx::Entity::serialize(entity));
        const auto described = tbx::Json::parse(tbx::Entity::describe(entity));

        // The component is keyed by its snake-cased wire name.
        const auto& lean_position = lean.at("components").at("transform").at("position");
        const auto& described_position = described.at("components").at("transform").at("position");

        // Serialize stays lean; describe grafts the reflection metadata back on for the editor.
        EXPECT_FALSE(lean_position.contains("category"));
        EXPECT_EQ(described_position.at("type"), "vec3");
        EXPECT_EQ(described_position.at("category"), "Transform");
        EXPECT_EQ(described_position.at("description"), "Local-space position, in metres.");

        // The entity id's read-only flag is re-added by describe but not persisted by serialize.
        EXPECT_FALSE(lean.at("id").contains("readonly"));
        EXPECT_TRUE(described.at("id").value("readonly", false));
    }

    TEST(ReflectionTests, ComponentIdPropertyIsHidden)
    {
        const auto info = tbx::reflect<tbx::Transform>();
        const auto id = info.property("id");
        ASSERT_TRUE(id.valid());
        ASSERT_NE(id.descriptor(), nullptr);
        EXPECT_TRUE(id.descriptor()->hidden);
    }

    TEST(ReflectionTests, DescribeRecursesIntoNestedArrayProperties)
    {
        auto registry = tbx::EntityRegistry();
        const auto id = registry.add("probe");
        auto& container = registry.add<tbx::ScriptContainer>(id);
        auto binding = tbx::ScriptContainerBinding {};
        binding.script.id = tbx::Uuid { 4242U };
        container.scripts.push_back(binding);
        const auto entity = registry.get(id);

        const auto described = tbx::Json::parse(tbx::Entity::describe(entity));
        const auto& component = described.at("components").at("script_container");

        // The component's own id is hidden at the top level.
        EXPECT_TRUE(component.at("id").value("hidden", false));

        // The nested binding's script reference is a handle (asset picker), and its binding_id — a
        // nested id — is tagged hidden by the recursive enrichment.
        const auto& first_binding = component.at("scripts").at("value").at(0);
        EXPECT_EQ(first_binding.at("script").at("type"), "handle");
        EXPECT_TRUE(first_binding.at("binding_id").value("hidden", false));
    }
}
