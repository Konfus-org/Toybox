#include "gtest/gtest.h"
#include "tbx/file_system/filesystem_ops.h"
#include "tbx/file_system/string_path_operations.h"
#include "tbx/logs/log_operations.h"
#include <filesystem>
#include <unordered_set>

namespace
{
    class FakeOps : public tbx::files::IFilesystemOps
    {
      public:
        explicit FakeOps(std::initializer_list<std::string> files)
        {
            for (const auto& entry : files)
            {
                existing.insert(entry);
            }
        }

        bool exists(const std::filesystem::path& path) const override
        {
            return existing.contains(path.generic_string());
        }

        bool is_directory(const std::filesystem::path&) const override
        {
            return false;
        }

        std::vector<tbx::files::DirectoryEntry>
            recursive_directory_entries(const std::filesystem::path&) const override
        {
            return {};
        }

        bool read_text_file(const std::filesystem::path&, std::string&) const override
        {
            return false;
        }

        bool remove(const std::filesystem::path& path) override
        {
            existing.erase(path.generic_string());
            return true;
        }

        bool rename(const std::filesystem::path& from, const std::filesystem::path& to) override
        {
            if (!exists(from))
            {
                return false;
            }
            existing.erase(from.generic_string());
            existing.insert(to.generic_string());
            return true;
        }

        bool copy(const std::filesystem::path& from, const std::filesystem::path& to) override
        {
            if (!exists(from))
            {
                return false;
            }
            existing.insert(to.generic_string());
            return true;
        }

        std::unordered_set<std::string> existing;
    };
}

TEST(LogStringPathOperationsTests, CalculatesLogPath)
{
    using tbx::logs::calculate_log_path;

    const auto active =
        calculate_log_path(std::filesystem::path("logs"), "AppName", 0).generic_string();
    const auto archived =
        calculate_log_path(std::filesystem::path("logs"), "AppName", 3).generic_string();

    EXPECT_EQ("logs/AppName.log", active);
    EXPECT_EQ("logs/AppName_3.log", archived);
}

TEST(LogStringPathOperationsTests, RotatesLogsUsingCustomOps)
{
    using tbx::logs::calculate_log_path;
    using tbx::logs::rotate_logs;

    FakeOps ops({
        calculate_log_path("logs", "App", 0).generic_string(),
        calculate_log_path("logs", "App", 1).generic_string(),
    });

    rotate_logs("logs", "App", 3, ops);

    EXPECT_TRUE(ops.exists(calculate_log_path("logs", "App", 1)));
    EXPECT_TRUE(ops.exists(calculate_log_path("logs", "App", 2)));
    EXPECT_FALSE(ops.exists(calculate_log_path("logs", "App", 0)));
}
