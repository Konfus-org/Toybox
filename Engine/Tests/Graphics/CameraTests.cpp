#include "PCH.h"
#include "Tbx/Graphics/Camera.h"
#include "Tbx/Math/Trig.h"

namespace Tbx::Tests::Graphics
{
    TEST(CameraTests, Constructor_InitializesWithDefaults)
    {
        // Arrange & Act
        Camera cam;

        // Assert
        EXPECT_TRUE(cam.IsPerspective());
        EXPECT_FALSE(cam.IsOrthagraphic());
        EXPECT_NEAR(cam.GetFov(), 60.0f, 1e-5f);
        EXPECT_NEAR(cam.GetZNear(), 0.1f, 1e-5f);
        EXPECT_NEAR(cam.GetZFar(), 1000.0f, 1e-5f);
        EXPECT_NEAR(cam.GetAspect(), 1.78f, 1e-5f);
    }

    TEST(CameraTests, SetOrthographic_SetsOrthoSettings)
    {
        // Arrange
        Camera cam;
        float size = 2.0f;
        float aspect = 1.5f;
        float zNear = 0.1f;
        float zFar = 100.0f;

        // Act
        cam.SetOrthagraphic(size, aspect, zNear, zFar);

        // Assert
        EXPECT_TRUE(cam.IsOrthagraphic());
        EXPECT_FALSE(cam.IsPerspective());
        EXPECT_NEAR(cam.GetZNear(), zNear, 1e-5f);
        EXPECT_NEAR(cam.GetZFar(), zFar, 1e-5f);
    }

    TEST(CameraTests, SetPerspective_SetsProjSettings)
    {
        // Arrange
        Camera cam;
        float fov = 90.0f;
        float aspect = 1.33f;
        float zNear = 0.5f;
        float zFar = 500.0f;

        // Act
        cam.SetPerspective(fov, aspect, zNear, zFar);

        // Assert
        EXPECT_TRUE(cam.IsPerspective());
        EXPECT_FALSE(cam.IsOrthagraphic());
        EXPECT_NEAR(cam.GetFov(), fov, 1e-5f);
        EXPECT_NEAR(cam.GetZNear(), zNear, 1e-5f);
        EXPECT_NEAR(cam.GetZFar(), zFar, 1e-5f);
    }

    TEST(CameraTests, SetAspect_UpdatesAspectRatioForPerspective)
    {
        // Arrange
        Camera cam;

        // Act
        cam.SetPerspective(60.0f, 1.0f, 0.1f, 100.0f);
        float newAspect = 2.0f;
        cam.SetAspect(newAspect);

        // Assert
        EXPECT_NEAR(cam.GetAspect(), newAspect, 1e-5f);
    }

    TEST(CameraTests, GetProjectionMatrix_ReturnsUpdatedMatrixAfterPerspectiveSet)
    {
        // Arrange
        Camera camera;
        float fov = 60.0f;
        float aspect = 1.78f;
        float zNear = 0.1f;
        float zFar = 100.0f;
        auto expected = Mat4x4::PerspectiveProjection(Math::DegreesToRadians(fov), aspect, zNear, zFar);

        // Act
        camera.SetPerspective(fov, aspect, zNear, zFar);
        const Mat4x4& projection = camera.GetProjectionMatrix();

        // Assert
        EXPECT_EQ(projection.ToString(), expected.ToString());
    }

    TEST(CameraTests, GetProjectionMatrix_ReturnsUpdatedMatrixAfterOrthoSet)
    {
        // Arrange
        Camera cam;
        float size = 2.0f;
        float aspect = 1.5f;
        float zNear = 0.1f;
        float zFar = 100.0f;
        auto expected = Mat4x4::OrthographicProjection(Bounds::FromOrthographicProjection(size, aspect), zNear, zFar);

        // Act
        cam.SetOrthagraphic(size, aspect, zNear, zFar);
        const Mat4x4& matrix = cam.GetProjectionMatrix();

        // Assert
        EXPECT_EQ(matrix.ToString(), expected.ToString());
    }

    TEST(CameraTests, CalculateViewMatrix_FromZeroPosAndRot)
    {
        // Arrange
        Vector3 camPosition(0, 0, 0);
        Quaternion camRotation = Quaternion::Identity;
        Mat4x4 expected = Mat4x4::Identity;

        // Act
        Mat4x4 result = Camera::CalculateViewMatrix(camPosition, camRotation);

        // Assert
        ASSERT_EQ(result, Mat4x4::Identity);
    }

    TEST(CameraTests, CalculateViewMatrix_TranslationOnly)
    {
        // Arrange
        Vector3 position(1.0f, 2.0f, 3.0f);
        Quaternion rotation = Quaternion::Identity;

        // Act
        Mat4x4 result = Camera::CalculateViewMatrix(position, rotation);

        // Assert
        Mat4x4 expected = 
        {
            {1.0f, 0.0f, 0.0f, -1.0f},
            {0.0f, 1.0f, 0.0f, -2.0f},
            {0.0f, 0.0f, 1.0f, -3.0f},
            {0.0f, 0.0f, 0.0f, 1.0f }
        };
        ASSERT_EQ(result.ToString(), expected.ToString());
    }

    TEST(CameraTests, CalculateViewProjectionMatrix_FromZeroPosAndRot)
    {
        // Arrange
        Vector3 camPosition(0, 0, 0);
        Quaternion camRotation = Quaternion::Identity;
        Mat4x4 projection = Mat4x4::Identity;

        // Act
        Mat4x4 result = Camera::CalculateViewProjectionMatrix(camPosition, camRotation, projection);

        // Assert
        ASSERT_EQ(result.ToString(), Mat4x4::Identity.ToString());
    }
}