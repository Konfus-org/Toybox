#include "gtest/gtest.h"
#include "tbx/tsl/string.h"

TEST(StringModsTests, TrimStringRemovesWhitespace)
{
    const tbx::String trimmed_full = tbx::get_trimmed("\t  content  \n");
    EXPECT_TRUE(trimmed_full == "content");

    const tbx::String trimmed_empty = tbx::get_trimmed("   ");
    EXPECT_TRUE(trimmed_empty == "");
}

TEST(StringModsTests, ToLowerCaseStringConvertsAllCharacters)
{
    const tbx::String lowered = tbx::get_lower_case("Hello World 123");
    EXPECT_TRUE(lowered == "hello world 123");

    const tbx::String lowered_mixed = tbx::get_lower_case("Mixed_Case_Value");
    EXPECT_TRUE(lowered_mixed == "mixed_case_value");
}
