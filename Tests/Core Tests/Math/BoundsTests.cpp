#include "PCH.h"
#include <Tbx/Core/Math/Bounds.h>
#include <Tbx/Core/Math/Trig.h>
#include <Tbx/Core/Math/Constants.h>

namespace Tbx::Tests::Core::Math
{
    TEST(BoundsTests, Constructor_SetsValuesCorrectly)
    {
        // Arrange
        float left = -2.0f;
        float right = 2.0f;
        float top = 1.5f;
        float bottom = -1.5f;

        // Act
        Bounds bounds(left, right, top, bottom);

        // Assert
        EXPECT_FLOAT_EQ(bounds.Left, -2.0f);
        EXPECT_FLOAT_EQ(bounds.Right, 2.0f);
        EXPECT_FLOAT_EQ(bounds.Top, 1.5f);
        EXPECT_FLOAT_EQ(bounds.Bottom, -1.5f);
    }

    TEST(BoundsTests, ToString_ProducesFormattedOutput)
    {
        // Arrange
        Bounds bounds(-2.0f, 2.0f, 1.5f, -1.5f);

        // Act
        std::string str = bounds.ToString();

        // Assert
        EXPECT_EQ(str, "[Left: -2, Right: 2, Top: 1.5, Bottom: -1.5]");
    }

    TEST(BoundsTests, Identity_ReturnsDefaultBounds)
    {
        // Act
        Bounds bounds = Constants::Bounds::Identity;

        // Assert
        EXPECT_FLOAT_EQ(bounds.Left, -1.0f);
        EXPECT_FLOAT_EQ(bounds.Right, 1.0f);
        EXPECT_FLOAT_EQ(bounds.Top, -1.0f);
        EXPECT_FLOAT_EQ(bounds.Bottom, 1.0f);
    }

    TEST(BoundsTests, FromOrthographicProjection_CreatesCorrectBounds)
    {
        // Arrange
        float size = 2.0f;
        float aspect = 1.5f;

        // Act
        Bounds bounds = Bounds::FromOrthographicProjection(size, aspect);

        // Assert
        float expectedWidth = size * aspect;
        EXPECT_FLOAT_EQ(bounds.Left, -expectedWidth);
        EXPECT_FLOAT_EQ(bounds.Right, expectedWidth);
        EXPECT_FLOAT_EQ(bounds.Top, size);
        EXPECT_FLOAT_EQ(bounds.Bottom, -size);
    }

    TEST(BoundsTests, FromPerspectiveProjection_ProducesCorrectBounds)
    {
        // Arrange
        float fov = Tbx::Math::DegreesToRadians(90.0f); // π/2 radians
        float aspect = 1.0f;
        float zNear = 1.0f;

        // Act
        Bounds bounds = Bounds::FromPerspectiveProjection(fov, aspect, zNear);

        // Assert
        float halfHeight = zNear * std::tan(fov / 2.0f);
        float halfWidth = halfHeight * aspect;

        EXPECT_NEAR(bounds.Left, -halfWidth, 1e-5f);
        EXPECT_NEAR(bounds.Right, halfWidth, 1e-5f);
        EXPECT_NEAR(bounds.Top, halfHeight, 1e-5f);
        EXPECT_NEAR(bounds.Bottom, -halfHeight, 1e-5f);
    }
}
