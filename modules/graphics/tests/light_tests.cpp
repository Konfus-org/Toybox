#include "PCH.h"
#include "tbx/graphics/light.h"

namespace tbx::tests::graphics
{
    // Validates default Light settings.
    TEST(LightTests, Constructor_InitializesWithDefaults)
    {
        // Arrange
        Light light = {};

        // Act
        // Defaults are set on construction.

        // Assert
        EXPECT_EQ(light.mode, LightMode::Point);
        EXPECT_NEAR(light.color.r, 1.0f, 1e-5f);
        EXPECT_NEAR(light.color.g, 1.0f, 1e-5f);
        EXPECT_NEAR(light.color.b, 1.0f, 1e-5f);
        EXPECT_NEAR(light.color.a, 1.0f, 1e-5f);
        EXPECT_NEAR(light.intensity, 1.0f, 1e-5f);
        EXPECT_NEAR(light.range, 10.0f, 1e-5f);
        EXPECT_NEAR(light.inner_angle, 20.0f, 1e-5f);
        EXPECT_NEAR(light.outer_angle, 35.0f, 1e-5f);
        EXPECT_NEAR(light.area_size.x, 1.0f, 1e-5f);
        EXPECT_NEAR(light.area_size.y, 1.0f, 1e-5f);
    }
}
