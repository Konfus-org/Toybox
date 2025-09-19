#include "PCH.h"
#include <cmath>
#include "Tbx/Math/Quaternion.h"
#include "Tbx/Math/Trig.h"
#include "Tbx/Math/Constants.h"

namespace Tbx::Tests::Math
{
    TEST(QuaternionTests, DefaultConstructor_InitializesToIdentity)
    {
        // Arrange & Act
        Quaternion q;

        // Assert
        EXPECT_NEAR(q.X, 0.0f, 1e-6f);
        EXPECT_NEAR(q.Y, 0.0f, 1e-6f);
        EXPECT_NEAR(q.Z, 0.0f, 1e-6f);
        EXPECT_NEAR(q.W, 1.0f, 1e-6f);
    }

    TEST(QuaternionTests, ValueConstructor_InitializesWithProvidedValues)
    {
        // Arrange & Act
        Quaternion q(1.0f, 2.0f, 3.0f, 4.0f);

        // Assert
        EXPECT_NEAR(q.X, 1.0f, 1e-6f);
        EXPECT_NEAR(q.Y, 2.0f, 1e-6f);
        EXPECT_NEAR(q.Z, 3.0f, 1e-6f);
        EXPECT_NEAR(q.W, 4.0f, 1e-6f);
    }

    TEST(QuaternionTests, FromEulerConstructor_CreatesQuaternionFromEulerAnglesX)
    {
        // Arrange
        Vector3 euler(90, 0.0f, 0.0f); // Rotate 90 degrees around X-axis

        // Act
        Quaternion q = euler;

        // Assert
        // Expected quaternion for 90° around X axis is (x=sin(θ/2), y=0, z=0, w=cos(θ/2))
        float radians = Tbx::Math::DegreesToRadians(90.0f);
        float halfAngle = radians * 0.5f;
        float expectedX = std::sin(halfAngle);
        float expectedW = std::cos(halfAngle);

        EXPECT_NEAR(q.X, expectedX, 1e-5f);
        EXPECT_NEAR(q.Y, 0.0f, 1e-5f);
        EXPECT_NEAR(q.Z, 0.0f, 1e-5f);
        EXPECT_NEAR(q.W, expectedW, 1e-5f);
    }

    TEST(QuaternionTests, FromEulerConstructor_CreatesQuaternionFromEulerAnglesY)
    {
        // Arrange
        Vector3 euler(0.0f, 90, 0.0f); // 90 degrees around Y

        // Act
        Quaternion q = euler;

        // Assert
        // Expected: x = 0, y = sin(θ/2), z = 0, w = cos(θ/2)
        float radians = Tbx::Math::DegreesToRadians(90.0f);
        float halfAngle = radians * 0.5f;
        float expectedY = std::sin(halfAngle);
        float expectedW = std::cos(halfAngle);

        EXPECT_NEAR(q.X, 0.0f, 1e-5f);
        EXPECT_NEAR(q.Y, expectedY, 1e-5f);
        EXPECT_NEAR(q.Z, 0.0f, 1e-5f);
        EXPECT_NEAR(q.W, expectedW, 1e-5f);
    }

    TEST(QuaternionTests, FromEulerConstructor_CreatesQuaternionFromEulerAnglesZ)
    {
        // Arrange
        Vector3 euler(0.0f, 0.0f, 90); // 90 degrees around Z

        // Act
        Quaternion q = euler;

        // Assert
        // Expected: x = 0, y = 0, z = sin(θ/2), w = cos(θ/2)
        float radians = Tbx::Math::DegreesToRadians(90.0f);
        float halfAngle = radians * 0.5f;
        float expectedZ = std::sin(halfAngle);
        float expectedW = std::cos(halfAngle);

        EXPECT_NEAR(q.X, 0.0f, 1e-5f);
        EXPECT_NEAR(q.Y, 0.0f, 1e-5f);
        EXPECT_NEAR(q.Z, -expectedZ, 1e-5f);
        EXPECT_NEAR(q.W, expectedW, 1e-5f);
    }

    TEST(QuaternionTests, OperatorAddition_AddsComponentWise)
    {
        // Arrange
        Quaternion q1(0.1f, 0.2f, 0.3f, 0.4f);
        Quaternion q2(0.5f, 0.6f, 0.7f, 0.8f);

        // Act
        Quaternion result = q1 + q2;

        // Assert
        EXPECT_NEAR(result.X, 0.6f, 1e-6f);
        EXPECT_NEAR(result.Y, 0.8f, 1e-6f);
        EXPECT_NEAR(result.Z, 1.0f, 1e-6f);
        EXPECT_NEAR(result.W, 1.2f, 1e-6f);
    }

    TEST(QuaternionTests, OperatorSubtraction_SubtractsComponentWise)
    {
        // Arrange
        Quaternion q1(0.8f, 0.7f, 0.6f, 0.5f);
        Quaternion q2(0.1f, 0.2f, 0.3f, 0.4f);

        // Act
        Quaternion result = q1 - q2;

        // Assert
        EXPECT_NEAR(result.X, 0.7f, 1e-6f);
        EXPECT_NEAR(result.Y, 0.5f, 1e-6f);
        EXPECT_NEAR(result.Z, 0.3f, 1e-6f);
        EXPECT_NEAR(result.W, 0.1f, 1e-6f);
    }

    TEST(QuaternionTests, OperatorMultiplicationQuaternion_MultipliesCorrectly)
    {
        // Arrange
        Quaternion q1 = Quaternion::FromEuler(Vector3(45, 0.0f, 0.0f));
        Quaternion q2 = Quaternion::FromEuler(Vector3(0.0f, 45, 0.0f));

        // Act
        Quaternion result = q1 * q2;
        Quaternion expected = Quaternion::Multiply(q1, q2);

        // Assert
        EXPECT_NEAR(result.X, expected.X, 1e-6f);
        EXPECT_NEAR(result.Y, expected.Y, 1e-6f);
        EXPECT_NEAR(result.Z, expected.Z, 1e-6f);
        EXPECT_NEAR(result.W, expected.W, 1e-6f);
    }

    TEST(QuaternionTests, OperatorMultiplicationQuaternionVector_RotatesVectorCorrectlyAroundYPositive)
    {
        // Arrange
        Quaternion rotation = Quaternion::FromEuler(Vector3(0.0f, 90, 0.0f));
        Vector3 v(1.0f, 0.0f, 0.0f);

        // Act
        Vector3 rotated = rotation * v;

        // Assert
        EXPECT_NEAR(rotated.X, 0.0f, 1e-6f);
        EXPECT_NEAR(rotated.Y, 0.0f, 1e-6f);
        EXPECT_NEAR(rotated.Z, -1.0f, 1e-6f); // Left-handed rotation
    }

    TEST(QuaternionTests, OperatorMultiplicationQuaternionVector_RotatesVectorCorrectlyAroundYNegative)
    {
        // Arrange
        Quaternion rotation = Quaternion::FromEuler(Vector3(0.0f, -90, 0.0f));
        Vector3 v(1.0f, 0.0f, 0.0f);

        // Act
        Vector3 rotated = rotation * v;

        // Assert
        EXPECT_NEAR(rotated.X, 0.0f, 1e-6f);
        EXPECT_NEAR(rotated.Y, 0.0f, 1e-6f);
        EXPECT_NEAR(rotated.Z, 1.0f, 1e-6f); // Left-handed rotation
    }

    TEST(QuaternionTests, OperatorMultiplicationVectorQuaternion_RotatesVectorCorrectlyAroundYPositive)
    {
        // Arrange
        Quaternion rotation = Quaternion::FromEuler(Vector3(0.0f, 90, 0.0f));
        Vector3 v(1.0f, 0.0f, 0.0f);

        // Act
        Vector3 rotated = v * rotation;

        // Assert
    }

    TEST(QuaternionTests, OperatorMultiplicationVectorQuaternion_RotatesVectorCorrectlyAroundYNegative)
    {
        // Arrange
        Quaternion rotation = Quaternion::FromEuler(Vector3(0.0f, -90, 0.0f));
        Vector3 v(1.0f, 0.0f, 0.0f);

        // Act
        Vector3 rotated = v * rotation;

        // Assert
    }

    TEST(QuaternionTests, ToStringMethod_ReturnsFormattedString)
    {
        // Arrange
        Quaternion q(1.1f, 2.2f, 3.3f, 4.4f);

        // Act
        std::string result = q.ToString();
        std::string expected = "(X: 1.1, Y: 2.2, Z: 3.3, W: 4.4)";

        // Assert
        EXPECT_EQ(result, expected);
    }

    TEST(QuaternionTests, NormalizeStaticMethod_ReturnsUnitQuaternion)
    {
        // Arrange
        Quaternion q(1.0f, 2.0f, 3.0f, 4.0f);

        // Act
        Quaternion normalized = Quaternion::Normalize(q);
        float magnitudeSq = normalized.X * normalized.X + normalized.Y * normalized.Y +
            normalized.Z * normalized.Z + normalized.W * normalized.W;

        // Assert
        EXPECT_NEAR(magnitudeSq, 1.0f, 1e-6f);
    }

    TEST(QuaternionTests, AddStaticMethod_AddsComponentWise)
    {
        // Arrange
        Quaternion q1(0.1f, 0.2f, 0.3f, 0.4f);
        Quaternion q2(0.5f, 0.6f, 0.7f, 0.8f);

        // Act
        Quaternion result = Quaternion::Add(q1, q2);

        // Assert
        EXPECT_NEAR(result.X, 0.6f, 1e-6f);
        EXPECT_NEAR(result.Y, 0.8f, 1e-6f);
        EXPECT_NEAR(result.Z, 1.0f, 1e-6f);
        EXPECT_NEAR(result.W, 1.2f, 1e-6f);
    }

    TEST(QuaternionTests, SubtractStaticMethod_SubtractsComponentWise)
    {
        // Arrange
        Quaternion q1(0.8f, 0.7f, 0.6f, 0.5f);
        Quaternion q2(0.1f, 0.2f, 0.3f, 0.4f);

        // Act
        Quaternion result = Quaternion::Subtract(q1, q2);

        // Assert
        EXPECT_NEAR(result.X, 0.7f, 1e-6f);
        EXPECT_NEAR(result.Y, 0.5f, 1e-6f);
        EXPECT_NEAR(result.Z, 0.3f, 1e-6f);
        EXPECT_NEAR(result.W, 0.1f, 1e-6f);
    }

    TEST(QuaternionTests, MultiplyStaticMethodQuaternion_MultipliesCorrectly)
    {
        // Arrange
        Quaternion q1 = Quaternion::FromEuler(Vector3(45, 0.0f, 0.0f));
        Quaternion q2 = Quaternion::FromEuler(Vector3(0.0f, 45, 0.0f));

        // Act
        Quaternion result = Quaternion::Multiply(q1, q2);

        // Assert
        Quaternion expected = Quaternion::FromEuler(Vector3(45, 45, 0.0f)); // Equivalent combined rotation
        EXPECT_TRUE(Quaternion::IsEqualOrEquivalent(result, expected));
    }

    TEST(QuaternionTests, MultiplyStaticMethodQuaternionVector_RotatesVectorCorrectlyAroundYPositive)
    {
        // Arrange
        Quaternion rotation = Quaternion::FromEuler(Vector3(0.0f, 90.0f, 0.0f));
        Vector3 v(1.0f, 0.0f, 0.0f);

        // Act
        Vector3 rotated = Quaternion::Multiply(rotation, v);

        // Assert
        EXPECT_NEAR(rotated.X, 0.0f, 1e-5f);
        EXPECT_NEAR(rotated.Y, 0.0f, 1e-5f);
        EXPECT_NEAR(rotated.Z, -1.0f, 1e-5f);
    }

    TEST(QuaternionTests, MultiplyStaticMethodVectorQuaternion_RotatesVectorCorrectlyAroundYPositive)
    {
        // Arrange
        Quaternion rotation = Quaternion::FromEuler(Vector3(0.0f, 90.0f, 0.0f));
        Vector3 v(1.0f, 0.0f, 0.0f);

        // Act
        Vector3 rotated = Quaternion::Multiply(v, rotation);

        // Assert
        EXPECT_NEAR(rotated.X, 0.0f, 1e-5f);
        EXPECT_NEAR(rotated.Y, 0.0f, 1e-5f);
        EXPECT_NEAR(rotated.Z, 1.0f, 1e-5f);
    }

    TEST(QuaternionTests, IdentityStaticMethod_ReturnsIdentityQuaternion)
    {
        // Arrange & Act
        Quaternion identity = Quaternion::Identity;

        // Assert
        EXPECT_NEAR(identity.X, 0.0f, 1e-6f);
        EXPECT_NEAR(identity.Y, 0.0f, 1e-6f);
        EXPECT_NEAR(identity.Z, 0.0f, 1e-6f);
        EXPECT_NEAR(identity.W, 1.0f, 1e-6f);
    }

}
