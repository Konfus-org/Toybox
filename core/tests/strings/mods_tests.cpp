#include "gtest/gtest.h"
#include "tbx/tsl/string.h"

TEST(StringModsTests, TrimStringRemovesWhitespace)
{
    const std::string trimmed_full = tbx::get_trimmed("\t  content  \n").std_string();
    EXPECT_EQ("content", trimmed_full);

    const std::string trimmed_empty = tbx::get_trimmed("   ").std_string();
    EXPECT_EQ("", trimmed_empty);
}

TEST(StringModsTests, ToLowerCaseStringConvertsAllCharacters)
{
    const std::string lowered = tbx::get_lower_case("Hello World 123").std_string();
    EXPECT_EQ("hello world 123", lowered);

    const std::string lowered_mixed =
        tbx::get_lower_case("Mixed_Case_Value").std_string();
    EXPECT_EQ("mixed_case_value", lowered_mixed);
}
