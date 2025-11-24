#include "pch.h"
#include "tbx/common/string_extensions.h"
#include <string_view>

namespace tbx::tests::common
{
    TEST(StringExtensionsTests, TrimsWhitespaceFromBothEnds)
    {
        std::string_view source = "  spaced text  ";

        auto result = trim_string(source);

        EXPECT_EQ(result, "spaced text");
    }

    TEST(StringExtensionsTests, ConvertsTextToLowerCase)
    {
        std::string_view source = "MiXeD";

        auto result = to_lower_case_string(source);

        EXPECT_EQ(result, "mixed");
    }
}
