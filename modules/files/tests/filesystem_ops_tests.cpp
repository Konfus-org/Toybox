#include "pch.h"
#include "tbx/files/file_ops.h"
#include <filesystem>

namespace tbx::tests::file_system
{
    TEST(FileOperatorTests, UsesProvidedWorkingDirectory)
    {
        const std::filesystem::path working("/tmp/tbx_working_root/");
        FileOperator ops = FileOperator(working);

        EXPECT_EQ(ops.get_working_directory(), working.lexically_normal());
    }

    TEST(FileOperatorTests, ResolvesRelativePathsAgainstWorkingDirectory)
    {
        const std::filesystem::path working("/tmp/tbx_resolve_root");
        FileOperator ops = FileOperator(working);

        std::filesystem::path relative("nested/file.txt");
        auto resolved = ops.resolve(relative);

        EXPECT_EQ(resolved, working / relative);
    }

    TEST(FileOperatorTests, LeavesAbsolutePathsUnchanged)
    {
        const std::filesystem::path working("/tmp/tbx_resolve_root");
        FileOperator ops = FileOperator(working);

        std::filesystem::path absolute("/var/log/app.txt");
        auto resolved = ops.resolve(absolute);

        EXPECT_EQ(resolved, absolute);
    }
}
