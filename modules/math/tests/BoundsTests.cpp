#include "PCH.h"
#include <cmath>
#include "tbx/math/bounds.h"
#include "tbx/math/trig.h"

namespace tbx::tests::math
{
    TEST(BoundsTests, Constructor_SetsValuesCorrectly)
    {
        float left = -2.0f;
        float right = 2.0f;
        float top = 1.5f;
        float bottom = -1.5f;

        Bounds bounds(left, right, top, bottom);

        EXPECT_FLOAT_EQ(bounds.Left, -2.0f);
        EXPECT_FLOAT_EQ(bounds.Right, 2.0f);
        EXPECT_FLOAT_EQ(bounds.Top, 1.5f);
        EXPECT_FLOAT_EQ(bounds.Bottom, -1.5f);
    }

    TEST(BoundsTests, ToString_ProducesFormattedOutput)
    {
        Bounds bounds(-2.0f, 2.0f, 1.5f, -1.5f);

        std::string str = to_string(bounds);

        EXPECT_EQ(str, "[Left: -2, Right: 2, Top: 1.5, Bottom: -1.5]");
    }

    TEST(BoundsTests, Identity_ReturnsDefaultBounds)
    {
        Bounds bounds = Bounds::Identity;

        EXPECT_FLOAT_EQ(bounds.Left, -1.0f);
        EXPECT_FLOAT_EQ(bounds.Right, 1.0f);
        EXPECT_FLOAT_EQ(bounds.Top, -1.0f);
        EXPECT_FLOAT_EQ(bounds.Bottom, 1.0f);
    }

    TEST(BoundsTests, FromOrthographicProjection_CreatesCorrectBounds)
    {
        float size = 2.0f;
        float aspect = 1.5f;

        Bounds bounds = Bounds::FromOrthographicProjection(size, aspect);

        float expectedWidth = size * aspect;
        EXPECT_FLOAT_EQ(bounds.Left, -expectedWidth);
        EXPECT_FLOAT_EQ(bounds.Right, expectedWidth);
        EXPECT_FLOAT_EQ(bounds.Top, size);
        EXPECT_FLOAT_EQ(bounds.Bottom, -size);
    }

    TEST(BoundsTests, FromPerspectiveProjection_ProducesCorrectBounds)
    {
        float fov = degrees_to_radians(90.0f);
        float aspect = 1.0f;
        float zNear = 1.0f;

        Bounds bounds = Bounds::FromPerspectiveProjection(fov, aspect, zNear);

        float halfHeight = zNear * std::tan(fov / 2.0f);
        float halfWidth = halfHeight * aspect;

        EXPECT_NEAR(bounds.Left, -halfWidth, 1e-5f);
        EXPECT_NEAR(bounds.Right, halfWidth, 1e-5f);
        EXPECT_NEAR(bounds.Top, halfHeight, 1e-5f);
        EXPECT_NEAR(bounds.Bottom, -halfHeight, 1e-5f);
    }
}
