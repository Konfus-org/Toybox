#include "pch.h"
#include "tbx/common/string_utils.h"
#include <array>
#include <string>
#include <string_view>

namespace tbx::tests::common
{
    TEST(StringUtilsTests, TrimsWhitespaceFromBothEnds)
    {
        const std::string value = "  spaced text  ";

        const auto trimmed = trim(value);

        EXPECT_EQ(trimmed, "spaced text");
        EXPECT_EQ(value, "  spaced text  ");
    }

    TEST(StringUtilsTests, RemovesAllWhitespace)
    {
        const std::string value = " a\tb\nc d ";

        const auto collapsed = remove_whitespace(value);

        EXPECT_EQ(collapsed, "abcd");
    }

    TEST(StringUtilsTests, ConvertsToLowerAndUpper)
    {
        const std::string value = "MiXeD";

        const auto lower = to_lower(value);
        const auto upper = to_upper(value);

        EXPECT_EQ(lower, "mixed");
        EXPECT_EQ(upper, "MIXED");
    }

    TEST(StringUtilsTests, ReplacesSubstringsAndCharacters)
    {
        const std::string value = "prefix-body-suffix";

        const auto replaced = replace(value, "body", "core");
        EXPECT_EQ(replaced, "prefix-core-suffix");

        const std::array<char, 2> characters = {'<', '>'};
        const auto swapped = replace("a<b>c", characters, '_');
        EXPECT_EQ(swapped, "a_b_c");
    }

    TEST(StringUtilsTests, RemovesSubstringsAndCharacters)
    {
        const std::string value = "path/./file";

        const auto stripped = remove(value, std::string_view{"./"});
        EXPECT_EQ(stripped, "path/file");

        const auto removed_char = remove("a-b-c", '-');
        EXPECT_EQ(removed_char, "abc");

        const std::array<char, 2> vowels = {'a', 'e'};
        const auto removed_chars = remove("peach", vowels);
        EXPECT_EQ(removed_chars, "pch");
    }
}
