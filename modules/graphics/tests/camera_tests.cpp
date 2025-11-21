#include "PCH.h"
#include "tbx/graphics/camera.h"
#include "tbx/math/matrices.h"
#include "tbx/math/quaternions.h"
#include "tbx/math/trig.h"
#include "tbx/math/vectors.h"
#include <cmath>

namespace tbx::tests::graphics
{
    bool matrices_close(const Mat4& lhs, const Mat4& rhs)
    {
        for (int column = 0; column < 4; ++column)
        {
            for (int row = 0; row < 4; ++row)
            {
                const float difference = std::fabs(lhs[column][row] - rhs[column][row]);
                if (difference > 1e-4f)
                {
                    return false;
                }
            }
        }

        return true;
    }

    TEST(CameraTests, Constructor_InitializesWithDefaults)
    {
        Camera camera;

        EXPECT_TRUE(camera.is_perspective());
        EXPECT_FALSE(camera.is_orthographic());
        EXPECT_NEAR(camera.get_fov(), 60.0f, 1e-5f);
        EXPECT_NEAR(camera.get_z_near(), 0.1f, 1e-5f);
        EXPECT_NEAR(camera.get_z_far(), 1000.0f, 1e-5f);
        EXPECT_NEAR(camera.get_aspect(), 1.78f, 1e-5f);
    }

    TEST(CameraTests, SetOrthographic_SetsOrthoSettings)
    {
        Camera camera;

        camera.set_orthographic(2.0f, 1.5f, 0.1f, 100.0f);

        EXPECT_TRUE(camera.is_orthographic());
        EXPECT_FALSE(camera.is_perspective());
        EXPECT_NEAR(camera.get_z_near(), 0.1f, 1e-5f);
        EXPECT_NEAR(camera.get_z_far(), 100.0f, 1e-5f);
    }

    TEST(CameraTests, SetPerspective_SetsProjSettings)
    {
        Camera camera;

        camera.set_perspective(90.0f, 1.33f, 0.5f, 500.0f);

        EXPECT_TRUE(camera.is_perspective());
        EXPECT_FALSE(camera.is_orthographic());
        EXPECT_NEAR(camera.get_fov(), 90.0f, 1e-5f);
        EXPECT_NEAR(camera.get_z_near(), 0.5f, 1e-5f);
        EXPECT_NEAR(camera.get_z_far(), 500.0f, 1e-5f);
    }

    TEST(CameraTests, SetAspect_UpdatesAspectRatioForPerspective)
    {
        Camera camera;
        camera.set_perspective(60.0f, 1.0f, 0.1f, 100.0f);

        camera.set_aspect(2.0f);

        EXPECT_NEAR(camera.get_aspect(), 2.0f, 1e-5f);
    }

    TEST(CameraTests, GetProjectionMatrix_ReturnsUpdatedMatrixAfterPerspectiveSet)
    {
        Camera camera;
        const float fov = 60.0f;
        const float aspect = 1.78f;
        const float z_near = 0.1f;
        const float z_far = 100.0f;

        camera.set_perspective(fov, aspect, z_near, z_far);

        const Mat4 expected = perspective_projection(degrees_to_radians(fov), aspect, z_near, z_far);

        EXPECT_TRUE(matrices_close(camera.get_projection_matrix(), expected));
    }

    TEST(CameraTests, GetProjectionMatrix_ReturnsUpdatedMatrixAfterOrthoSet)
    {
        Camera camera;
        const float size = 2.0f;
        const float aspect = 1.5f;
        const float z_near = 0.1f;
        const float z_far = 100.0f;

        camera.set_orthographic(size, aspect, z_near, z_far);

        const float half_size = size * 0.5f;
        const float top = half_size;
        const float bottom = -half_size;
        const float right = top * aspect;
        const float left = -right;
        const Mat4 expected = ortho_projection(left, right, bottom, top, z_near, z_far);

        EXPECT_TRUE(matrices_close(camera.get_projection_matrix(), expected));
    }

    TEST(CameraTests, CalculateViewProjectionMatrix_FromZeroPosAndRot)
    {
        const Vec3 cam_position(0.0f, 0.0f, 0.0f);
        const Quat cam_rotation = Quat(Vec3(0.0f));
        const Mat4 projection(1.0f);

        const Mat4 result = get_camera_view_projection_matrix(cam_position, cam_rotation, projection);

        EXPECT_TRUE(matrices_close(result, projection));
    }
}
