#include "PCH.h"
#include "tbx/graphics/light.h"

namespace tbx::tests::graphics
{
    // Validates default PointLight settings.
    TEST(LightTests, PointLight_Constructor_InitializesWithDefaults)
    {
        // Arrange
        PointLight light = {};

        // Act
        // Defaults are set on construction.

        // Assert
        EXPECT_NEAR(light.color.r, 1.0f, 1e-5f);
        EXPECT_NEAR(light.color.g, 1.0f, 1e-5f);
        EXPECT_NEAR(light.color.b, 1.0f, 1e-5f);
        EXPECT_NEAR(light.color.a, 1.0f, 1e-5f);
        EXPECT_NEAR(light.intensity, 1.0f, 1e-5f);
        EXPECT_NEAR(light.range, 10.0f, 1e-5f);
    }

    // Validates default SpotLight settings.
    TEST(LightTests, SpotLight_Constructor_InitializesWithDefaults)
    {
        // Arrange
        SpotLight light = {};

        // Act
        // Defaults are set on construction.

        // Assert
        EXPECT_NEAR(light.color.r, 1.0f, 1e-5f);
        EXPECT_NEAR(light.color.g, 1.0f, 1e-5f);
        EXPECT_NEAR(light.color.b, 1.0f, 1e-5f);
        EXPECT_NEAR(light.color.a, 1.0f, 1e-5f);
        EXPECT_NEAR(light.intensity, 1.0f, 1e-5f);
        EXPECT_NEAR(light.range, 10.0f, 1e-5f);
        EXPECT_NEAR(light.inner_angle, 20.0f, 1e-5f);
        EXPECT_NEAR(light.outer_angle, 35.0f, 1e-5f);
    }

    // Validates default AreaLight settings.
    TEST(LightTests, AreaLight_Constructor_InitializesWithDefaults)
    {
        // Arrange
        AreaLight light = {};

        // Act
        // Defaults are set on construction.

        // Assert
        EXPECT_NEAR(light.color.r, 1.0f, 1e-5f);
        EXPECT_NEAR(light.color.g, 1.0f, 1e-5f);
        EXPECT_NEAR(light.color.b, 1.0f, 1e-5f);
        EXPECT_NEAR(light.color.a, 1.0f, 1e-5f);
        EXPECT_NEAR(light.intensity, 1.0f, 1e-5f);
        EXPECT_NEAR(light.range, 10.0f, 1e-5f);
        EXPECT_NEAR(light.area_size.x, 1.0f, 1e-5f);
        EXPECT_NEAR(light.area_size.y, 1.0f, 1e-5f);
    }

    // Validates default DirectionalLight settings.
    TEST(LightTests, DirectionalLight_Constructor_InitializesWithDefaults)
    {
        // Arrange
        DirectionalLight light = {};

        // Act
        // Defaults are set on construction.

        // Assert
        EXPECT_NEAR(light.color.r, 1.0f, 1e-5f);
        EXPECT_NEAR(light.color.g, 1.0f, 1e-5f);
        EXPECT_NEAR(light.color.b, 1.0f, 1e-5f);
        EXPECT_NEAR(light.color.a, 1.0f, 1e-5f);
        EXPECT_NEAR(light.intensity, 1.0f, 1e-5f);
        EXPECT_NEAR(light.ambient, 0.03f, 1e-5f);
    }
}
