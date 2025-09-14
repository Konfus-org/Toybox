#include "PCH.h"
#include "Tbx/Math/Vectors.h"
#include "Tbx/Math/Constants.h"

namespace Tbx::Tests::Math
{
    TEST(Vector3Tests, ToString_ProducesFormattedOutput)
    {
        // Arrange
        Vector3 v(1.0f, 2.0f, 3.0f);

        // Act
        std::string str = v.ToString();

        // Assert
        EXPECT_EQ(str, "(1, 2, 3)");
    }

    TEST(Vector3Tests, OperatorAdd_AddsTwoVectors)
    {
        // Arrange
        Vector3 a(1, 2, 3);
        Vector3 b(4, 5, 6);

        // Act
        Vector3 result = a + b;

        // Assert
        EXPECT_FLOAT_EQ(result.X, 5);
        EXPECT_FLOAT_EQ(result.Y, 7);
        EXPECT_FLOAT_EQ(result.Z, 9);
    }

    TEST(Vector3Tests, OperatorSubtract_SubtractsTwoVectors)
    {
        // Arrange
        Vector3 a(4, 5, 6);
        Vector3 b(1, 2, 3);

        // Act
        Vector3 result = a - b;

        // Assert
        EXPECT_FLOAT_EQ(result.X, 3);
        EXPECT_FLOAT_EQ(result.Y, 3);
        EXPECT_FLOAT_EQ(result.Z, 3);
    }

    TEST(Vector3Tests, OperatorMultiply_MultipliesTwoVectorsComponentWise)
    {
        // Arrange
        Vector3 a(2, 3, 4);
        Vector3 b(5, 6, 7);

        // Act
        Vector3 result = a * b;

        // Assert
        EXPECT_FLOAT_EQ(result.X, 10);
        EXPECT_FLOAT_EQ(result.Y, 18);
        EXPECT_FLOAT_EQ(result.Z, 28);
    }

    TEST(Vector3Tests, Normalize_ReturnsNormalizedVector)
    {
        // Arrange
        Vector3 v(3, 0, 4);

        // Act
        Vector3 result = Vector3::Normalize(v);

        // Assert
        EXPECT_NEAR(result.X, 0.6f, 1e-5f);
        EXPECT_NEAR(result.Y, 0.0f, 1e-5f);
        EXPECT_NEAR(result.Z, 0.8f, 1e-5f);
    }

    TEST(Vector3Tests, Dot_ReturnsDotProduct)
    {
        // Arrange
        Vector3 a(1, 2, 3);
        Vector3 b(4, -5, 6);

        // Act
        float dot = Vector3::Dot(a, b);

        // Assert
        EXPECT_FLOAT_EQ(dot, 12); // 1*4 + 2*(-5) + 3*6 = 12
    }

    TEST(Vector3Tests, Cross_ReturnsCrossProduct)
    {
        // Arrange
        Vector3 a(1, 0, 0);
        Vector3 b(0, 1, 0);

        // Act
        Vector3 result = Vector3::Cross(a, b);

        // Assert
        EXPECT_FLOAT_EQ(result.X, 0);
        EXPECT_FLOAT_EQ(result.Y, 0);
        EXPECT_FLOAT_EQ(result.Z, 1);
    }

    TEST(Vector2Tests, Constructor_FromVector3_CopiesXY)
    {
        // Arrange
        Vector3 v3(1.5f, 2.5f, 3.0f);

        // Act
        Vector2 v2(v3);

        // Assert
        EXPECT_FLOAT_EQ(v2.X, 1.5f);
        EXPECT_FLOAT_EQ(v2.Y, 2.5f);
    }

    TEST(Vector2Tests, Zero_ReturnsZeroVector)
    {
        // Act
        Vector2 v = Consts::Vector2::Zero;

        // Assert
        EXPECT_FLOAT_EQ(v.X, 0);
        EXPECT_FLOAT_EQ(v.Y, 0);
    }

    TEST(Vector2Tests, ToString_ProducesFormattedOutput)
    {
        // Arrange
        Vector2 v(1.0f, 2.0f);

        // Act
        std::string str = v.ToString();

        // Assert
        EXPECT_EQ(str, "(1, 2)");
    }

    TEST(Vector2Tests, OperatorAdd_AddsTwoVectors)
    {
        // Arrange
        Vector2 a(1, 2);
        Vector2 b(4, 5);

        // Act
        Vector2 result = a + b;

        // Assert
        EXPECT_FLOAT_EQ(result.X, 5);
        EXPECT_FLOAT_EQ(result.Y, 7);
    }

    TEST(Vector2Tests, OperatorSubtract_SubtractsTwoVectors)
    {
        // Arrange
        Vector2 a(4, 5);
        Vector2 b(1, 2);

        // Act
        Vector2 result = a - b;

        // Assert
        EXPECT_FLOAT_EQ(result.X, 3);
        EXPECT_FLOAT_EQ(result.Y, 3);
    }

    TEST(Vector2Tests, OperatorMultiply_MultipliesTwoVectorsComponentWise)
    {
        // Arrange
        Vector2 a(2, 3);
        Vector2 b(5, 6);

        // Act
        Vector2 result = a * b;

        // Assert
        EXPECT_FLOAT_EQ(result.X, 10);
        EXPECT_FLOAT_EQ(result.Y, 18);
    }

    TEST(Vector2Tests, Normalize_ReturnsNormalizedVector)
    {
        // Arrange
        Vector2 v(3, 4);

        // Act
        Vector2 result = Vector2::Normalize(v);

        // Assert
        EXPECT_NEAR(result.X, 0.6f, 1e-5f);
        EXPECT_NEAR(result.Y, 0.8f, 1e-5f);
    }

    TEST(Vector2Tests, Dot_ReturnsDotProduct)
    {
        // Arrange
        Vector2 a(1, 2);
        Vector2 b(4, -5);

        // Act
        float dot = Vector2::Dot(a, b);

        // Assert
        EXPECT_FLOAT_EQ(dot, -6); // 1*4 + 2*(-5) = -6
    }

    TEST(Vector2ITests, Constructor_FromVector3_CastsAndCopiesXY)
    {
        // Arrange
        Vector3 v3(1.9f, 2.1f, 3.7f);

        // Act
        Vector2I v2i(v3);

        // Assert
        EXPECT_EQ(v2i.X, 1);
        EXPECT_EQ(v2i.Y, 2);
    }

    TEST(Vector2ITests, Identity_ReturnsVectorWithOnes)
    {
        // Act
        Vector2I v = Consts::Vector2I::Identity;

        // Assert
        EXPECT_EQ(v.X, 1);
        EXPECT_EQ(v.Y, 1);
    }

    TEST(Vector2ITests, ToString_ProducesFormattedOutput)
    {
        // Arrange
        Vector2I v(1, 2);

        // Act
        std::string str = v.ToString();

        // Assert
        EXPECT_EQ(str, "(1, 2)");
    }

    TEST(Vector2ITests, OperatorAdd_AddsTwoVectors)
    {
        // Arrange
        Vector2I a(1, 2);
        Vector2I b(4, 5);

        // Act
        Vector2I result = a + b;

        // Assert
        EXPECT_EQ(result.X, 5);
        EXPECT_EQ(result.Y, 7);
    }

    TEST(Vector2ITests, OperatorSubtract_SubtractsTwoVectors)
    {
        // Arrange
        Vector2I a(4, 5);
        Vector2I b(1, 2);

        // Act
        Vector2I result = a - b;

        // Assert
        EXPECT_EQ(result.X, 3);
        EXPECT_EQ(result.Y, 3);
    }

    TEST(Vector2ITests, OperatorMultiply_MultipliesTwoVectorsComponentWise)
    {
        // Arrange
        Vector2I a(2, 3);
        Vector2I b(5, 6);

        // Act
        Vector2I result = a * b;

        // Assert
        EXPECT_EQ(result.X, 10);
        EXPECT_EQ(result.Y, 18);
    }

    TEST(Vector2ITests, Dot_ReturnsDotProduct)
    {
        // Arrange
        Vector2I a(1, 2);
        Vector2I b(4, -5);

        // Act
        int dot = Vector2I::Dot(a, b);

        // Assert
        EXPECT_EQ(dot, -6); // 1*4 + 2*(-5) = -6
    }
}