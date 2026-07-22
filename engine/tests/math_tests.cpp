#include "tbx/math/frustum.h"
#include "tbx/math/math.h"
#include <gtest/gtest.h>

namespace tbx::tests
{
    /// @brief
    /// Purpose: A camera at the origin looking down -Z: 60 degree fov, square aspect,
    /// 0.1..100 depth — the shape every case below reasons against.
    static Frustum test_frustum()
    {
        const Mat4 view_projection =
            perspective(radians(60.0f), 1.0f, 0.1f, 100.0f)
            * look_at(Vec3(0.0f), Vec3(0.0f, 0.0f, -1.0f), Vec3(0.0f, 1.0f, 0.0f));
        return make_frustum(view_projection);
    }

    TEST(Frustum, SphereInViewIntersects)
    {
        const auto frustum = test_frustum();
        EXPECT_TRUE(intersects(frustum, Vec3(0.0f, 0.0f, -10.0f), 1.0f));
    }

    TEST(Frustum, SpheresOutsideEveryPlaneDoNot)
    {
        const auto frustum = test_frustum();
        EXPECT_FALSE(intersects(frustum, Vec3(-100.0f, 0.0f, -10.0f), 1.0f)); // left
        EXPECT_FALSE(intersects(frustum, Vec3(100.0f, 0.0f, -10.0f), 1.0f)); // right
        EXPECT_FALSE(intersects(frustum, Vec3(0.0f, -100.0f, -10.0f), 1.0f)); // below
        EXPECT_FALSE(intersects(frustum, Vec3(0.0f, 100.0f, -10.0f), 1.0f)); // above
        EXPECT_FALSE(intersects(frustum, Vec3(0.0f, 0.0f, 10.0f), 1.0f)); // behind
        EXPECT_FALSE(intersects(frustum, Vec3(0.0f, 0.0f, -200.0f), 1.0f)); // beyond far
    }

    TEST(Frustum, StraddlingSphereIntersects)
    {
        // Center outside the left plane, but the sphere is big enough to reach back in.
        const auto frustum = test_frustum();
        const auto center = Vec3(-7.0f, 0.0f, -10.0f);
        EXPECT_FALSE(intersects(frustum, center, 0.25f));
        EXPECT_TRUE(intersects(frustum, center, 5.0f));
    }

    TEST(Frustum, MarginInflationIsMetric)
    {
        // The streaming margins add metres to the radius; that only works if the extracted
        // planes are unit-length. A sphere ~3.7m outside the left plane must intersect once
        // the radius is inflated by 5m (the load margin) and not before.
        const auto frustum = test_frustum();
        const auto center = Vec3(-10.0f, 0.0f, -10.0f);
        constexpr float RADIUS = 1.0f;
        constexpr float LOAD_MARGIN = 5.0f;
        EXPECT_FALSE(intersects(frustum, center, RADIUS));
        EXPECT_TRUE(intersects(frustum, center, RADIUS + LOAD_MARGIN));
    }
}
