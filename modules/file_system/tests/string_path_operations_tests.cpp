#include "gtest/gtest.h"
#include "tbx/file_system/string_path_operations.h"
#include <string>

TEST(FileStringPathOperationsTests, SanitizesUnsupportedCharacters)
{
    const std::string input = "Hello<>:\"/\\\\|?*World";
    EXPECT_EQ("Hello_________World", tbx::sanitize_string_for_file_name_usage(input));
}

TEST(FileStringPathOperationsTests, LeavesValidCharactersUntouched)
{
    const std::string input = "Valid_Name-123";
    EXPECT_EQ(input, tbx::sanitize_string_for_file_name_usage(input));
}

TEST(FileStringPathOperationsTests, ReplacesEmptyWithDefault)
{
    EXPECT_EQ(std::string("unnamed"), tbx::sanitize_string_for_file_name_usage(""));
}

TEST(FileStringPathOperationsTests, ExtractsFilenameOnly)
{
    EXPECT_EQ(std::string("file.txt"), tbx::get_filename_from_string_path("C:/tmp/file.txt"));
    EXPECT_EQ(std::string("another.log"), tbx::get_filename_from_string_path("/var/log/another.log"));
    EXPECT_TRUE(tbx::get_filename_from_string_path("").empty());
}
