#include "pch.h"
#include "tbx/file_system/filesystem.h"
#include <filesystem>
#include <string>

namespace tbx::tests::file_system
{
    static std::filesystem::path make_temp_dir(const std::string& token)
    {
        auto base = std::filesystem::temp_directory_path()
                    / std::filesystem::path("tbx_fs_ops_test_" + token + "_%%%%%%");
        auto unique = std::filesystem::unique_path(base);
        std::filesystem::create_directories(unique);
        return unique;
    }

    TEST(FileSystemOpsTests, WritesAndReadsUtf8Files)
    {
        FileSystem fs;
        auto dir = make_temp_dir("utf8");
        FilePath path(dir / "sample.txt");

        ASSERT_TRUE(fs.write_file(path, "Hello Unicode", FileDataFormat::Utf8Text));

        std::string contents;
        ASSERT_TRUE(fs.read_file(path, contents, FileDataFormat::Utf8Text));
        EXPECT_EQ(contents, "Hello Unicode");

        std::filesystem::remove_all(dir);
    }

    TEST(FileSystemOpsTests, WritesAndReadsBinaryFiles)
    {
        FileSystem fs;
        auto dir = make_temp_dir("binary");
        FilePath path(dir / "sample.bin");

        const std::string payload("\x01\x02\x00\x03", 4);
        ASSERT_TRUE(fs.write_file(path, payload, FileDataFormat::Binary));

        std::string contents;
        ASSERT_TRUE(fs.read_file(path, contents, FileDataFormat::Binary));
        ASSERT_EQ(contents.size(), payload.size());
        EXPECT_EQ(contents[0], '\x01');
        EXPECT_EQ(contents[1], '\x02');
        EXPECT_EQ(contents[2], '\0');
        EXPECT_EQ(contents[3], '\x03');

        std::filesystem::remove_all(dir);
    }

    TEST(FileSystemOpsTests, UsesWorkingDirectoryForDefaultFolders)
    {
        const auto working = make_temp_dir("working_root");
        FileSystem fs(FilePath(working));

        EXPECT_EQ(fs.get_working_directory(), FilePath(working));
        EXPECT_EQ(fs.get_plugins_directory(), FilePath(working / "plugins"));
        EXPECT_EQ(fs.get_logs_directory(), FilePath(working / "logs"));
        EXPECT_EQ(fs.get_assets_directory(), FilePath(working / "assets"));

        std::filesystem::remove_all(working);
    }

    TEST(FileSystemOpsTests, UsesProvidedDirectoriesWhenSpecified)
    {
        const auto working = make_temp_dir("working_root");
        const auto plugins_override = working / "custom_plugins";
        const auto logs_override = working / "custom_logs";
        const auto assets_override = working / "custom_assets";

        FileSystem fs(
            FilePath(working),
            FilePath(plugins_override),
            FilePath(logs_override),
            FilePath(assets_override));

        EXPECT_EQ(fs.get_working_directory(), FilePath(working));
        EXPECT_EQ(fs.get_plugins_directory(), FilePath(plugins_override));
        EXPECT_EQ(fs.get_logs_directory(), FilePath(logs_override));
        EXPECT_EQ(fs.get_assets_directory(), FilePath(assets_override));

        std::filesystem::remove_all(working);
    }

    TEST(FileSystemOpsTests, CreatesDirectoriesRelativeToWorkingRoot)
    {
        const auto working = make_temp_dir("create_dir");
        FileSystem fs(FilePath(working));

        FilePath relative("new/logs");
        ASSERT_TRUE(fs.create_directory(relative));

        auto resolved = fs.resolve_relative_path(relative);
        EXPECT_TRUE(std::filesystem::is_directory(resolved.std_path()));

        std::filesystem::remove_all(working);
    }

    TEST(FileSystemOpsTests, CreatesFilesAndParents)
    {
        const auto working = make_temp_dir("create_file");
        FileSystem fs(FilePath(working));

        FilePath relative("nested/file.txt");
        ASSERT_TRUE(fs.create_file(relative));

        auto resolved = fs.resolve_relative_path(relative);
        EXPECT_TRUE(std::filesystem::exists(resolved.std_path()));

        std::filesystem::remove_all(working);
    }
}
