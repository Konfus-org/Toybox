#include "PCH.h"
#include "tbx/math/bounds.h"
#include "tbx/math/trig.h"
#include <cmath>

namespace tbx::tests::math
{
    TEST(BoundsTests, Constructor_SetsValuesCorrectly)
    {
        float left = -2.0f;
        float right = 2.0f;
        float top = 1.5f;
        float bottom = -1.5f;

        Bounds bounds(left, right, top, bottom);

        EXPECT_FLOAT_EQ(bounds.left, -2.0f);
        EXPECT_FLOAT_EQ(bounds.right, 2.0f);
        EXPECT_FLOAT_EQ(bounds.top, 1.5f);
        EXPECT_FLOAT_EQ(bounds.bottom, -1.5f);
    }

    TEST(BoundsTests, ToString_ProducesFormattedOutput)
    {
        Bounds bounds(-2.0f, 2.0f, 1.5f, -1.5f);

        String str = to_string(bounds);

        EXPECT_EQ(str, "[Left: -2, Right: 2, Top: 1.5, Bottom: -1.5]");
    }

    TEST(BoundsTests, FromOrthographicProjection_CreatesCorrectBounds)
    {
        float size = 2.0f;
        float aspect = 1.5f;

        Bounds bounds = Bounds::from_orthographic_projection(size, aspect);

        float expectedWidth = size * aspect;
        EXPECT_FLOAT_EQ(bounds.left, -expectedWidth);
        EXPECT_FLOAT_EQ(bounds.right, expectedWidth);
        EXPECT_FLOAT_EQ(bounds.top, size);
        EXPECT_FLOAT_EQ(bounds.bottom, -size);
    }

    TEST(BoundsTests, FromPerspectiveProjection_ProducesCorrectBounds)
    {
        float fov = degrees_to_radians(90.0f);
        float aspect = 1.0f;
        float zNear = 1.0f;

        Bounds bounds = Bounds::from_perspective_projection(fov, aspect, zNear);

        float halfHeight = zNear * std::tan(fov / 2.0f);
        float halfWidth = halfHeight * aspect;

        EXPECT_NEAR(bounds.left, -halfWidth, 1e-5f);
        EXPECT_NEAR(bounds.right, halfWidth, 1e-5f);
        EXPECT_NEAR(bounds.top, halfHeight, 1e-5f);
        EXPECT_NEAR(bounds.bottom, -halfHeight, 1e-5f);
    }
}
