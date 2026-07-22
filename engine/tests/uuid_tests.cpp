#include "tbx/utils/uuid.h"
#include <gtest/gtest.h>

namespace tbx::tests
{
    TEST(Uuid, RoundTripsThroughString)
    {
        // Arrange
        const Uuid original = Uuid::generate();

        // Act
        const Uuid parsed = Uuid::parse(original.to_string());

        // Assert
        EXPECT_EQ(parsed, original);
    }

    TEST(Uuid, ParseRejectsMalformedText)
    {
        // Arrange
        const auto malformed = std::string("not-a-uuid");

        // Act
        const Uuid parsed = Uuid::parse(malformed);

        // Assert
        EXPECT_FALSE(parsed.is_valid());
    }

    TEST(Uuid, GenerateProducesDistinctNonNilIds)
    {
        // Arrange / Act
        const Uuid first = Uuid::generate();
        const Uuid second = Uuid::generate();

        // Assert
        EXPECT_TRUE(first.is_valid());
        EXPECT_NE(first, second);
    }

    TEST(Uuid, ParseRejectsCorrectLengthNonHexText)
    {
        // Arrange
        const auto non_hex = std::string(32, 'z');

        // Act
        const Uuid parsed = Uuid::parse(non_hex);

        // Assert
        EXPECT_FALSE(parsed.is_valid());
    }
}
