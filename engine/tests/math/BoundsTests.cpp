#include "tbx/types/bounds.h"
#include "tbx/types/trig.h"

namespace tbx::tests::math
{
    TEST(BoundsTests, ConstructedBounds_PreserveProjectionEdges)
    {
        // Arrange
        const float left = -2.0f;
        const float right = 2.0f;
        const float top = 1.5f;
        const float bottom = -1.5f;

        // Act
        const Bounds bounds(left, right, top, bottom);

        // Assert
        EXPECT_FLOAT_EQ(bounds.left, -2.0f);
        EXPECT_FLOAT_EQ(bounds.right, 2.0f);
        EXPECT_FLOAT_EQ(bounds.top, 1.5f);
        EXPECT_FLOAT_EQ(bounds.bottom, -1.5f);
    }

    TEST(BoundsTests, ToString_ProducesFormattedOutput)
    {
        // Arrange
        const Bounds bounds(-2.0f, 2.0f, 1.5f, -1.5f);

        // Act
        const std::string str = std::format("{}", bounds);

        // Assert
        EXPECT_EQ(str, "[Left: -2, Right: 2, Top: 1.5, Bottom: -1.5]");
    }

    TEST(BoundsTests, FromOrthographicProjection_CreatesCorrectBounds)
    {
        // Arrange
        const float size = 2.0f;
        const float aspect = 1.5f;

        // Act
        const Bounds bounds = Bounds::from_orthographic_projection(size, aspect);

        // Assert
        const float expected_width = size * aspect;
        EXPECT_FLOAT_EQ(bounds.left, -expected_width);
        EXPECT_FLOAT_EQ(bounds.right, expected_width);
        EXPECT_FLOAT_EQ(bounds.top, size);
        EXPECT_FLOAT_EQ(bounds.bottom, -size);
    }

    TEST(BoundsTests, FromPerspectiveProjection_ProducesCorrectBounds)
    {
        // Arrange
        const float fov = to_radians(90.0f);
        const float aspect = 1.0f;
        const float z_near = 1.0f;

        // Act
        const Bounds bounds = Bounds::from_perspective_projection(fov, aspect, z_near);

        // Assert
        const float half_height = z_near * std::tan(fov / 2.0f);
        const float half_width = half_height * aspect;

        EXPECT_NEAR(bounds.left, -half_width, 1e-5f);
        EXPECT_NEAR(bounds.right, half_width, 1e-5f);
        EXPECT_NEAR(bounds.top, half_height, 1e-5f);
        EXPECT_NEAR(bounds.bottom, -half_height, 1e-5f);
    }
}
