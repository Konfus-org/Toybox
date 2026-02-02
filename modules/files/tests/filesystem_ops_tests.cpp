#include "pch.h"
#include "tbx/files/filesystem.h"
#include <filesystem>

namespace tbx::tests::file_system
{
    TEST(FileSystemOpsTests, UsesWorkingDirectoryForDefaultFolders)
    {
        const std::filesystem::path working("/tmp/tbx_working_root/");
        FileSystem fs {working};

        EXPECT_EQ(fs.get_working_directory(), working.lexically_normal());
        EXPECT_EQ(fs.get_plugins_directory(), working.lexically_normal());
        EXPECT_EQ(fs.get_logs_directory(), working / "logs");
        const auto assets_directories = fs.get_assets_directories();
        ASSERT_EQ(assets_directories.size(), 1U);
        EXPECT_EQ(assets_directories.back(), working / "assets"); // the front is the built-in
    }

    TEST(FileSystemOpsTests, UsesProvidedDirectoriesWhenSpecified)
    {
        const std::filesystem::path working("/tmp/tbx_custom_root");
        const std::filesystem::path plugins_override = working / "custom_plugins";
        const std::filesystem::path logs_override = working / "custom_logs";
        const std::filesystem::path assets_override = working / "custom_assets";

        auto fs = FileSystem(working, plugins_override, logs_override, {assets_override});

        EXPECT_EQ(fs.get_working_directory(), working);
        EXPECT_EQ(fs.get_plugins_directory(), plugins_override);
        EXPECT_EQ(fs.get_logs_directory(), logs_override);
        const auto assets_directories = fs.get_assets_directories();
        ASSERT_EQ(assets_directories.size(), 1U);
        EXPECT_EQ(assets_directories[0], assets_override);
    }

    TEST(FileSystemOpsTests, ResolvesRelativePathsAgainstWorkingDirectory)
    {
        const std::filesystem::path working("/tmp/tbx_resolve_root");
        FileSystem fs {working};

        std::filesystem::path relative("nested/file.txt");
        auto resolved = fs.resolve_relative_path(relative);

        EXPECT_EQ(resolved, working / relative);
    }

    TEST(FileSystemOpsTests, LeavesAbsolutePathsUnchanged)
    {
        const std::filesystem::path working("/tmp/tbx_resolve_root");
        FileSystem fs {working};

        std::filesystem::path absolute("/var/log/app.txt");
        auto resolved = fs.resolve_relative_path(absolute);

        EXPECT_EQ(resolved, absolute);
    }
}
