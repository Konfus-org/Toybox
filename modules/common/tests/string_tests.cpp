#include "pch.h"
#include "tbx/common/string_utils.h"
#include <array>
#include <string>

namespace tbx::tests::common
{
    TEST(StringUtilsTests, TrimsWhitespaceFromBothEnds)
    {
        const std::string value = "  spaced text  ";

        const auto trimmed = TrimString(value);

        EXPECT_EQ(trimmed, "spaced text");
        EXPECT_EQ(value, "  spaced text  ");
    }

    TEST(StringUtilsTests, RemovesAllWhitespace)
    {
        const std::string value = " a\tb\nc d ";

        const auto collapsed = RemoveWhitespace(value);

        EXPECT_EQ(collapsed, "abcd");
    }

    TEST(StringUtilsTests, ConvertsToLowerAndUpper)
    {
        const std::string value = "MiXeD";

        const auto lower = ToLower(value);
        const auto upper = ToUpper(value);

        EXPECT_EQ(lower, "mixed");
        EXPECT_EQ(upper, "MIXED");
    }

    TEST(StringUtilsTests, ReplacesSubstringsAndCharacters)
    {
        const std::string value = "prefix-body-suffix";

        const auto replaced = ReplaceAll(value, "body", "core");
        EXPECT_EQ(replaced, "prefix-core-suffix");

        const std::array<char, 2> characters = {'<', '>'};
        const auto swapped = ReplaceCharacters("a<b>c", characters, '_');
        EXPECT_EQ(swapped, "a_b_c");
    }

    TEST(StringUtilsTests, RemovesSubstringsAndCharacters)
    {
        const std::string value = "path/./file";

        const auto stripped = RemoveAll(value, "./");
        EXPECT_EQ(stripped, "path/file");

        const auto removed_char = RemoveCharacter("a-b-c", '-');
        EXPECT_EQ(removed_char, "abc");

        const std::array<char, 2> vowels = {'a', 'e'};
        const auto removed_chars = RemoveCharacters("peach", vowels);
        EXPECT_EQ(removed_chars, "pch");
    }
}
