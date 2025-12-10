#include "pch.h"
#include "tbx/file_system/filesystem.h"
#include <filesystem>
#include <string>

namespace tbx::tests::file_system
{
    TEST(FileSystemOpsTests, UsesWorkingDirectoryForDefaultFolders)
    {
        const std::filesystem::path working("/tmp/tbx_working_root");
        FileSystem fs{FilePath(working)};

        EXPECT_EQ(fs.get_working_directory(), FilePath(working));
        EXPECT_EQ(fs.get_plugins_directory(), FilePath(working / "plugins"));
        EXPECT_EQ(fs.get_logs_directory(), FilePath(working / "logs"));
        EXPECT_EQ(fs.get_assets_directory(), FilePath(working / "assets"));
    }

    TEST(FileSystemOpsTests, UsesProvidedDirectoriesWhenSpecified)
    {
        const std::filesystem::path working("/tmp/tbx_custom_root");
        const std::filesystem::path plugins_override = working / "custom_plugins";
        const std::filesystem::path logs_override = working / "custom_logs";
        const std::filesystem::path assets_override = working / "custom_assets";

        FileSystem fs{
            FilePath(working),
            FilePath(plugins_override),
            FilePath(logs_override),
            FilePath(assets_override)};

        EXPECT_EQ(fs.get_working_directory(), FilePath(working));
        EXPECT_EQ(fs.get_plugins_directory(), FilePath(plugins_override));
        EXPECT_EQ(fs.get_logs_directory(), FilePath(logs_override));
        EXPECT_EQ(fs.get_assets_directory(), FilePath(assets_override));
    }

    TEST(FileSystemOpsTests, ResolvesRelativePathsAgainstWorkingDirectory)
    {
        const std::filesystem::path working("/tmp/tbx_resolve_root");
        FileSystem fs{FilePath(working)};

        FilePath relative("nested/file.txt");
        auto resolved = fs.resolve_relative_path(relative);

        EXPECT_EQ(resolved.std_path(), working / relative.std_path());
    }

    TEST(FileSystemOpsTests, LeavesAbsolutePathsUnchanged)
    {
        const std::filesystem::path working("/tmp/tbx_resolve_root");
        FileSystem fs{FilePath(working)};

        FilePath absolute("/var/log/app.txt");
        auto resolved = fs.resolve_relative_path(absolute);

        EXPECT_EQ(resolved.std_path(), absolute.std_path());
    }
}
