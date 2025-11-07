#include "gtest/gtest.h"
#include "tbx/strings/mods.h"

TEST(StringModsTests, TrimStringRemovesWhitespace)
{
    EXPECT_EQ("content", tbx::trim_string("\t  content  \n"));
    EXPECT_EQ("", tbx::trim_string("   "));
}

TEST(StringModsTests, ToLowerCaseStringConvertsAllCharacters)
{
    EXPECT_EQ("hello world 123", tbx::to_lower_case_string("Hello World 123"));
    EXPECT_EQ("mixed_case_value", tbx::to_lower_case_string("Mixed_Case_Value"));
}
