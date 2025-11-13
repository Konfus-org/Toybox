#include "gtest/gtest.h"
#include "tbx/file_system/string_path_operations.h"

namespace
{
    using tbx::sanitize_string_for_file_name_usage;
    using tbx::get_filename_from_string_path;
}

TEST(FileStringPathOperationsTests, SanitizesUnsupportedCharacters)
{
    const std::string input = "Hello<>:\"/\\\\|?*World";
    EXPECT_EQ("Hello_________World", sanitize_string_for_file_name_usage(input));
}

TEST(FileStringPathOperationsTests, LeavesValidCharactersUntouched)
{
    const std::string input = "Valid_Name-123";
    EXPECT_EQ(input, sanitize_string_for_file_name_usage(input));
}

TEST(FileStringPathOperationsTests, ReplacesEmptyWithDefault)
{
    EXPECT_EQ("unnamed", sanitize_string_for_file_name_usage(""));
}

TEST(FileStringPathOperationsTests, ExtractsFilenameOnly)
{
    EXPECT_EQ("file.txt", get_filename_from_string_path("C:/tmp/file.txt"));
    EXPECT_EQ("another.log", get_filename_from_string_path("/var/log/another.log"));
    EXPECT_TRUE(get_filename_from_string_path("").empty());
}
