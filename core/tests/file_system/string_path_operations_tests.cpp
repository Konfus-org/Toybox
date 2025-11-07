#include "gtest/gtest.h"
#include "tbx/file_system/string_path_operations.h"

namespace
{
    using tbx::files::sanitize_for_file_name_usage;
    using tbx::files::filename_only;
}

TEST(FileStringPathOperationsTests, SanitizesUnsupportedCharacters)
{
    const std::string input = "Hello<>:\"/\\\\|?*World";
    EXPECT_EQ("Hello_________World", sanitize_for_file_name_usage(input));
}

TEST(FileStringPathOperationsTests, LeavesValidCharactersUntouched)
{
    const std::string input = "Valid_Name-123";
    EXPECT_EQ(input, sanitize_for_file_name_usage(input));
}

TEST(FileStringPathOperationsTests, ReplacesEmptyWithDefault)
{
    EXPECT_EQ("unnamed", sanitize_for_file_name_usage(""));
}

TEST(FileStringPathOperationsTests, ExtractsFilenameOnly)
{
    EXPECT_EQ("file.txt", filename_only("C:/tmp/file.txt"));
    EXPECT_EQ("another.log", filename_only("/var/log/another.log"));
    EXPECT_TRUE(filename_only("").empty());
}
