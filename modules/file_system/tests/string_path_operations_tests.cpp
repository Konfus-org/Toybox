#include "gtest/gtest.h"
#include "tbx/file_system/string_path_operations.h"
#include "tbx/std/string.h"

TEST(FileStringPathOperationsTests, SanitizesUnsupportedCharacters)
{
    const tbx::String input = "Hello<>:\"/\\\\|?*World";
    EXPECT_EQ(tbx::String("Hello_________World"), tbx::sanitize_string_for_file_name_usage(input));
}

TEST(FileStringPathOperationsTests, LeavesValidCharactersUntouched)
{
    const tbx::String input = "Valid_Name-123";
    EXPECT_EQ(input, tbx::sanitize_string_for_file_name_usage(input));
}

TEST(FileStringPathOperationsTests, ReplacesEmptyWithDefault)
{
    EXPECT_EQ(tbx::String("unnamed"), tbx::sanitize_string_for_file_name_usage(""));
}

TEST(FileStringPathOperationsTests, ExtractsFilenameOnly)
{
    EXPECT_EQ(tbx::String("file.txt"), tbx::get_filename_from_string_path("C:/tmp/file.txt"));
    EXPECT_EQ(tbx::String("another.log"), tbx::get_filename_from_string_path("/var/log/another.log"));
    EXPECT_TRUE(tbx::get_filename_from_string_path("").is_empty());
}
