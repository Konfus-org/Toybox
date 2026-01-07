#include "pch.h"
#include "tbx/files/filesystem.h"
#include <filesystem>

namespace tbx::tests::file_system
{
    TEST(FileSystemOpsTests, UsesWorkingDirectoryForDefaultFolders)
    {
        const std::filesystem::path working("/tmp/tbx_working_root");
        FileSystem fs{working};

        EXPECT_EQ(fs.get_working_directory(), working);
        EXPECT_EQ(fs.get_plugins_directory(), working / "plugins");
        EXPECT_EQ(fs.get_logs_directory(), working / "logs");
        EXPECT_EQ(fs.get_assets_directory(), working / "assets");
    }

    TEST(FileSystemOpsTests, UsesProvidedDirectoriesWhenSpecified)
    {
        const std::filesystem::path working("/tmp/tbx_custom_root");
        const std::filesystem::path plugins_override = working / "custom_plugins";
        const std::filesystem::path logs_override = working / "custom_logs";
        const std::filesystem::path assets_override = working / "custom_assets";

        FileSystem fs{
            working,
            plugins_override,
            logs_override,
            assets_override};

        EXPECT_EQ(fs.get_working_directory(), working);
        EXPECT_EQ(fs.get_plugins_directory(), plugins_override);
        EXPECT_EQ(fs.get_logs_directory(), logs_override);
        EXPECT_EQ(fs.get_assets_directory(), assets_override);
    }

    TEST(FileSystemOpsTests, ResolvesRelativePathsAgainstWorkingDirectory)
    {
        const std::filesystem::path working("/tmp/tbx_resolve_root");
        FileSystem fs{working};

        std::filesystem::path relative("nested/file.txt");
        auto resolved = fs.resolve_relative_path(relative);

        EXPECT_EQ(resolved, working / relative);
    }

    TEST(FileSystemOpsTests, LeavesAbsolutePathsUnchanged)
    {
        const std::filesystem::path working("/tmp/tbx_resolve_root");
        FileSystem fs{working};

        std::filesystem::path absolute("/var/log/app.txt");
        auto resolved = fs.resolve_relative_path(absolute);

        EXPECT_EQ(resolved, absolute);
    }
}
