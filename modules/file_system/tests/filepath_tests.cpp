#include "gtest/gtest.h"
#include "tbx/file_system/filepath.h"
#include "tbx/common/string.h"
#include <chrono>
#include <filesystem>
#include <fstream>

static std::filesystem::path make_unique_path(const tbx::String& token)
{
    const auto timestamp = std::chrono::steady_clock::now().time_since_epoch().count();
    auto base = std::filesystem::temp_directory_path() / (std::string(token) + "_");
    return base / std::to_string(timestamp);
}

TEST(FilePathTests, SanitizesUnsupportedPathComponents)
{
    tbx::FilePath path(std::filesystem::path("logs/<bad>/output?.txt"));

    EXPECT_EQ(path.std_path(), std::filesystem::path("logs/_bad_/output_.txt"));
}

TEST(FilePathTests, ProvidesFilenameAndParent)
{
    tbx::FilePath path("dir/sub/file.txt");

    EXPECT_EQ(path.filename_string(), "file.txt");
    EXPECT_EQ(path.parent_path().std_path(), std::filesystem::path("dir/sub"));
}

TEST(FilePathTests, AppendsNewComponents)
{
    tbx::FilePath base("/root");

    auto combined = base.append("child").append("file.txt");

    EXPECT_EQ(combined.std_path(), std::filesystem::path("/root/child/file.txt"));
}

TEST(FilePathTests, ReplacesExtensionsAndHandlesEmptyInput)
{
    tbx::FilePath path("");

    EXPECT_EQ(path.std_path(), std::filesystem::path("unnamed"));

    tbx::FilePath ext_path("file.old");
    auto replaced = ext_path.set_extension(".txt");

    EXPECT_EQ(replaced.filename_string(), "file.txt");
}

TEST(FilePathTests, ReturnsFileExtension)
{
    tbx::FilePath path("file.meta");

    EXPECT_EQ(path.get_extension(), ".meta");
}

TEST(FilePathTests, DetectsFileTypes)
{
    auto unique_dir = make_unique_path("tbx_fp_type_test");
    std::filesystem::create_directories(unique_dir);

    tbx::FilePath dir_path(unique_dir);
    EXPECT_EQ(dir_path.get_file_type(), tbx::FilePathType::Directory);

    auto file_path = unique_dir / "sample.txt";
    {
        std::ofstream out(file_path);
        out << "data";
    }
    EXPECT_EQ(tbx::FilePath(file_path).get_file_type(), tbx::FilePathType::Regular);

    std::filesystem::remove_all(unique_dir);
}
