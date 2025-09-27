#include "PCH.h"
#include <cmath>
#include "Tbx/Math/Mat4x4.h"
#include "Tbx/Math/Trig.h"

namespace Tbx::Tests::Math
{
    TEST(Mat4x4Tests, ToString_ProducesReadableOutput)
    {
        // Arrange
        Mat4x4 m = Mat4x4::Identity;

        // Act
        std::string str = m.ToString();

        // Assert
        EXPECT_FALSE(str.empty());
        EXPECT_EQ("[1, 0, 0, 0],\n[0, 1, 0, 0],\n[0, 0, 1, 0],\n[0, 0, 0, 1]", str);
    }
    TEST(Mat4x4Tests, OnConstruct_EqualToIdentity)
    {
        EXPECT_EQ(Mat4x4().ToString(), Mat4x4::Identity.ToString());
    }

    TEST(Mat4x4Tests, Add_ProducesExpectedResult)
    {
        // Arrange
        Mat4x4 a = {
            1, 2, 3, 4,
            5, 6, 7, 8,
            9, 10, 11, 12,
            13, 14, 15, 16
        };

        Mat4x4 b = {
            16, 15, 14, 13,
            12, 11, 10, 9,
            8, 7, 6, 5,
            4, 3, 2, 1
        };

        Mat4x4 expected = {
            17, 17, 17, 17,
            17, 17, 17, 17,
            17, 17, 17, 17,
            17, 17, 17, 17
        };

        // Act
        Mat4x4 result = a + b;

        // Assert
        EXPECT_EQ(result.ToString(), expected.ToString());
    }

    TEST(Mat4x4Tests, Subtract_ProducesExpectedResult)
    {
        // Arrange
        Mat4x4 a = {
            16, 15, 14, 13,
            12, 11, 10, 9,
            8, 7, 6, 5,
            4, 3, 2, 1
        };

        Mat4x4 b = {
            1, 2, 3, 4,
            5, 6, 7, 8,
            9, 10, 11, 12,
            13, 14, 15, 16
        };

        Mat4x4 expected = {
            15, 13, 11, 9,
            7, 5, 3, 1,
            -1, -3, -5, -7,
            -9, -11, -13, -15
        };

        // Act
        Mat4x4 result = a - b;

        // Assert
        EXPECT_EQ(result.ToString(), expected.ToString());
    }

    TEST(Mat4x4Tests, Multiply_ProducesExpectedResult)
    {
        // Arrange
        Mat4x4 a = {
            1, 2, 3, 4,
            5, 6, 7, 8,
            9,10,11,12,
           13,14,15,16 
        };

        Mat4x4 b = Mat4x4::Identity;

        // Act
        Mat4x4 expected = a;

        Mat4x4 result = a * b;

        // Assert
        EXPECT_EQ(result.ToString(), expected.ToString());
    }

    TEST(Mat4x4Tests, MultiplyScalarLeft_ScalesAllElements)
    {
        // Arrange
        Mat4x4 m = Mat4x4::Identity;

        // Act
        Mat4x4 result = 2.0f * m;

        // Assert
        EXPECT_FLOAT_EQ(result(0, 0), 2.0f);
        EXPECT_FLOAT_EQ(result(1, 1), 2.0f);
        EXPECT_FLOAT_EQ(result(2, 2), 2.0f);
        EXPECT_FLOAT_EQ(result(3, 3), 2.0f);
    }

    TEST(Mat4x4Tests, MultiplyScalarRight_ScalesAllElements)
    {
        // Arrange
        Mat4x4 m = Mat4x4::Identity;

        // Act
        Mat4x4 result = m * 3.0f;

        // Assert
        EXPECT_FLOAT_EQ(result(0, 0), 3.0f);
        EXPECT_FLOAT_EQ(result(1, 1), 3.0f);
        EXPECT_FLOAT_EQ(result(2, 2), 3.0f);
        EXPECT_FLOAT_EQ(result(3, 3), 3.0f);
    }

    TEST(Mat4x4Tests, Zero_ReturnsAllZeroMatrix)
    {
        // Arrange & Act
        Mat4x4 zero = Mat4x4::Zero;

        // Assert
        for (int row = 0; row < 4; ++row)
            for (int col = 0; col < 4; ++col)
                EXPECT_FLOAT_EQ(zero(row, col), 0.0f);
    }

    TEST(Mat4x4Tests, Identity_ReturnsIdentityMatrix)
    {
        // Arrange & Act
        Mat4x4 identity = Mat4x4::Identity;

        // Assert
        for (int row = 0; row < 4; ++row)
            for (int col = 0; col < 4; ++col)
                EXPECT_FLOAT_EQ(identity(row, col), row == col ? 1.0f : 0.0f);
    }

    TEST(Mat4x4Tests, FromPosition_SetsTranslation)
    {
        // Arrange
        Vector3 position(3, 4, 5);

        // Act
        Mat4x4 m = Mat4x4::FromPosition(position);
        auto debug = m.ToString();

        // Assert
        EXPECT_FLOAT_EQ(m(3, 0), 3.0f);
        EXPECT_FLOAT_EQ(m(3, 1), 4.0f);
        EXPECT_FLOAT_EQ(m(3, 2), 5.0f);
    }

    TEST(Mat4x4Tests, FromRotation_AppliesQuaternionRotation)
    {
        // Arrange
        Quaternion rotation = Quaternion::FromEuler(0.0f, 90.0f, 0.0f);

        // Act
        Mat4x4 result = Mat4x4::FromRotation(rotation);

        auto dbg = result.ToString();

        // Assert
        EXPECT_NEAR(result[0], 0.0f, 0.001f);
        EXPECT_NEAR(result[1], 0.0f, 0.001f);
        EXPECT_NEAR(result[2], -1.0f, 0.001f);
        EXPECT_NEAR(result[3], 0.0f, 0.001f);

        EXPECT_NEAR(result[4], 0.0f, 0.001f);
        EXPECT_NEAR(result[5], 1.0f, 0.001f);
        EXPECT_NEAR(result[6], 0.0f, 0.001f);
        EXPECT_NEAR(result[7], 0.0f, 0.001f);

        EXPECT_NEAR(result[8], 1.0f, 0.001f);
        EXPECT_NEAR(result[9], 0.0f, 0.001f);
        EXPECT_NEAR(result[10], 0.0f, 0.001f);
        EXPECT_NEAR(result[11], 0.0f, 0.001f);

        EXPECT_NEAR(result[12], 0.0f, 0.001f);
        EXPECT_NEAR(result[13], 0.0f, 0.001f);
        EXPECT_NEAR(result[14], 0.0f, 0.001f);
        EXPECT_NEAR(result[15], 1.0f, 0.001f);
    }

    TEST(Mat4x4Tests, FromScale_SetsScaleOnDiagonal)
    {
        // Arrange
        Vector3 scale(2.0f, 3.0f, 4.0f);

        // Act
        Mat4x4 result = Mat4x4::FromScale(scale);

        // Assert
        EXPECT_FLOAT_EQ(result(0, 0), 2.0f);
        EXPECT_FLOAT_EQ(result(1, 1), 3.0f);
        EXPECT_FLOAT_EQ(result(2, 2), 4.0f);
        EXPECT_FLOAT_EQ(result(3, 3), 1.0f);
    }

    TEST(Mat4x4Tests, FromTRS_CombinesTranslateRotateScale)
    {
        // Arrange
        Vector3 pos(1.0f, 2.0f, 3.0f);
        Quaternion rot = Quaternion::FromEuler(0.0f, 90, 0.0f); // Convert to radians!
        Vector3 scale(2.0f, 2.0f, 2.0f);

        // Act
        Mat4x4 trs = Mat4x4::FromTRS(pos, rot, scale);

        // Assert
        EXPECT_NEAR(trs(0, 2), -2.0f, 0.001f);
        EXPECT_NEAR(trs(1, 1), 2.0f, 0.001f);
        EXPECT_NEAR(trs(2, 0), 2.0f, 0.001f);
        EXPECT_NEAR(trs(3, 0), 1.0f, 0.001f);
        EXPECT_NEAR(trs(3, 1), 2.0f, 0.001f);
        EXPECT_NEAR(trs(3, 2), 3.0f, 0.001f);
        EXPECT_NEAR(trs(3, 3), 1.0f, 0.001f);
    }

    TEST(Mat4x4Tests, LookAt_CreatesValidViewMatrix)
    {
        // Arrange
        Vector3 from(0.0f, 0.0f, 5.0f);
        Vector3 target(0.0f, 0.0f, 0.0f);
        Vector3 up(0.0f, 1.0f, 0.0f);

        // Act
        Mat4x4 viewMatrix = Mat4x4::LookAt(from, target, up);

        // Assert
        Vector3 forward = Vector3::Normalize(target - from);
        Vector3 right = Vector3::Normalize(Vector3::Cross(up, forward));
        Vector3 recalculatedUp = Vector3::Cross(forward, right);

        EXPECT_NEAR(Vector3::Dot(forward, Vector3(0, 0, -1)), 1.0f, 0.001f);
        EXPECT_NEAR(Vector3::Dot(recalculatedUp, up), 1.0f, 0.001f);
    }

    TEST(Mat4x4Tests, OrthographicProjection_ProducesExpectedMatrix)
    {
        // Arrange
        Bounds bounds{ -1.0f, 1.0f, 1.0f, -1.0f }; // left, right, top, bottom
        float zNear = 0.1f;
        float zFar = 100.0f;
        float epsilon = 1e-6f;

        // Act
        Mat4x4 ortho = Mat4x4::OrthographicProjection(bounds, zNear, zFar);

        // Assert
        float rl = bounds.Right - bounds.Left;
        float tb = bounds.Top - bounds.Bottom;
        float fn = zFar - zNear;

        EXPECT_NEAR(ortho(0, 0), 2.0f / rl, epsilon);
        EXPECT_NEAR(ortho(1, 0), 0.0f, epsilon);
        EXPECT_NEAR(ortho(2, 0), 0.0f, epsilon);
        EXPECT_NEAR(ortho(3, 0), 0.0f, epsilon);

        EXPECT_NEAR(ortho(0, 1), 0.0f, epsilon);
        EXPECT_NEAR(ortho(1, 1), 2.0f / tb, epsilon);
        EXPECT_NEAR(ortho(2, 1), 0.0f, epsilon);
        EXPECT_NEAR(ortho(3, 1), 0.0f, epsilon);

        EXPECT_NEAR(ortho(0, 2), 0.0f, epsilon);
        EXPECT_NEAR(ortho(1, 2), 0.0f, epsilon);
        EXPECT_NEAR(ortho(2, 2), 2.0f / fn, epsilon);
        EXPECT_NEAR(ortho(3, 2), -1.0f, epsilon); // Translation in Z is zero with near=0

        EXPECT_NEAR(ortho(0, 3), -(bounds.Left + bounds.Right) / rl, epsilon);
        EXPECT_NEAR(ortho(1, 3), -(bounds.Bottom + bounds.Top) / tb, epsilon);
        EXPECT_NEAR(ortho(2, 3), 0.0f, epsilon);
        EXPECT_NEAR(ortho(3, 3), 1.0f, epsilon);
    }

    TEST(Mat4x4Tests, PerspectiveProjection_ProducesExpectedMatrix)
    {
        // Arrange
        float fov = Tbx::Math::DegreesToRadians(60.0f);
        float aspect = 16.0f / 9.0f;
        float zNear = 0.1f;
        float zFar = 100.0f;
        float f = 1.0f / std::tan(fov / 2.0f);
        float epsilon = 1e-6f;

        // Act
        Mat4x4 perspective = Mat4x4::PerspectiveProjection(fov, aspect, zNear, zFar);

        // Assert
        EXPECT_NEAR(perspective(0, 0), f / aspect, epsilon);
        EXPECT_NEAR(perspective(1, 0), 0.0f, epsilon);
        EXPECT_NEAR(perspective(2, 0), 0.0f, epsilon);
        EXPECT_NEAR(perspective(3, 0), 0.0f, epsilon);

        EXPECT_NEAR(perspective(0, 1), 0.0f, epsilon);
        EXPECT_NEAR(perspective(1, 1), f, epsilon);
        EXPECT_NEAR(perspective(2, 1), 0.0f, epsilon);
        EXPECT_NEAR(perspective(3, 1), 0.0f, epsilon);

        EXPECT_NEAR(perspective(0, 2), 0.0f, epsilon);
        EXPECT_NEAR(perspective(1, 2), 0.0f, epsilon);
        EXPECT_NEAR(perspective(2, 2), -(zFar + zNear) / (zNear - zFar), epsilon);
        EXPECT_NEAR(perspective(3, 2), -(2.0f * zFar * zNear) / (zFar - zNear), epsilon);

        EXPECT_NEAR(perspective(0, 3), 0.0f, epsilon);
        EXPECT_NEAR(perspective(1, 3), 0.0f, epsilon);
        EXPECT_NEAR(perspective(2, 3), 1.0f, epsilon);
        EXPECT_NEAR(perspective(3, 3), 0.0f, epsilon);
    }
}

