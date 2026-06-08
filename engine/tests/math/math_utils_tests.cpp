#include "tbx/utils/math_utils.h"

namespace tbx::tests::math
{
    TEST(MathUtilsTests, DivideRoundUpReturnsExpectedGroupCount)
    {
        const uint32 value = 129U;
        const uint32 divisor = 64U;

        const uint32 result = divide_round_up(value, divisor);

        EXPECT_EQ(result, 3U);
    }

    TEST(MathUtilsTests, DivideRoundUpReturnsZeroForEmptyDomain)
    {
        const uint32 value = 0U;
        const uint32 divisor = 64U;

        const uint32 result = divide_round_up(value, divisor);

        EXPECT_EQ(result, 0U);
    }
}
