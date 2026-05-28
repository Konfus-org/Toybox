#include "tbx/systems/assets/serialization.h"
#include <typeindex>

namespace tbx::tests::assets
{
    TEST(
        SerializableTypeRegistrationTests,
        DuplicateRegistrationNamePreservesOriginalSerializerCallbacks)
    {
        // Arrange
        const auto registration_name = std::string("tbx_test_duplicate_registration_keeps_first");
        register_serializable_type_entry(
            SerializableTypeRegistration {
                .name = registration_name,
                .type_name = "FirstType",
                .type = std::type_index(typeid(int)),
                .write_value =
                    [](const void*)
                {
                    return std::string("first");
                },
                .read_value =
                    [](std::string_view, void*)
                {
                    return true;
                },
            });

        // Act
        register_serializable_type_entry(
            SerializableTypeRegistration {
                .name = registration_name,
                .type_name = "SecondType",
                .type = std::type_index(typeid(int)),
                .write_value =
                    [](const void*)
                {
                    return std::string("second");
                },
                .read_value =
                    [](std::string_view, void*)
                {
                    return false;
                },
            });

        const auto registrations = get_serializable_type_registrations();
        const auto existing = std::ranges::find_if(
            registrations,
            [&registration_name](const SerializableTypeRegistration& registration)
            {
                return registration.name == registration_name;
            });

        // Assert
        ASSERT_NE(existing, registrations.end());
        EXPECT_EQ(existing->write_value(nullptr), "first");
        EXPECT_TRUE(existing->read_value("", nullptr));
    }
}
