#include "tbx/systems/assets/serialization.h"
#include "tbx/systems/files/json.h"
#include "tbx/types/clamp.h"

// Positive: a value inside the bounds is kept verbatim on construction and assignment.
TEST(ClampTests, KeepsValuesInsideBounds)
{
    tbx::Clamp<int, 0, 100> value = 50;
    EXPECT_EQ(static_cast<int>(value), 50);

    value = 75;
    EXPECT_EQ(static_cast<int>(value), 75);
}

// Negative: out-of-range values clamp to the nearest bound, both on construction and assignment.
TEST(ClampTests, ClampsValuesOutsideBounds)
{
    tbx::Clamp<int, 10, 20> below = 5;
    EXPECT_EQ(static_cast<int>(below), 10);

    tbx::Clamp<int, 10, 20> above = 999;
    EXPECT_EQ(static_cast<int>(above), 20);

    below = -100;
    EXPECT_EQ(static_cast<int>(below), 10);

    above = 100;
    EXPECT_EQ(static_cast<int>(above), 20);
}

// Positive: a Clamp serializes to exactly the same JSON as its bare underlying value, so swapping a
// plain field for a Clamp is backwards compatible on disk and transparent to the editor.
TEST(ClampTests, SerializesIdenticallyToUnderlyingValue)
{
    const tbx::Clamp<uint32, 256U> clamped = 4096U;
    const auto clamped_json = tbx::write_serialization_value<tbx::Json>(clamped);
    const auto plain_json = tbx::write_serialization_value<tbx::Json>(uint32(4096U));
    EXPECT_EQ(clamped_json, plain_json);

    const auto clamped_token = tbx::get_property_type_token<tbx::Clamp<uint32, 256U>>();
    const auto plain_token = tbx::get_property_type_token<uint32>();
    EXPECT_EQ(clamped_token, plain_token);
}

// Negative: a value persisted before the field gained its floor (or any out-of-range wire value) is
// re-clamped when read back.
TEST(ClampTests, DeserializeReappliesBounds)
{
    const auto json = tbx::write_serialization_value<tbx::Json>(uint32(64U));

    tbx::Clamp<uint32, 256U> value = {};
    tbx::read_serialization_value(json, value);

    EXPECT_EQ(static_cast<uint32>(value), 256U);
}
