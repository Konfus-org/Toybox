#pragma once
#include "tbx/files/filesystem.h"
#include <filesystem>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace tbx::tests::assets
{
    class TestFileSystem final : public IFileSystem
    {
      public:
        void add_directory(const std::filesystem::path& path)
        {
            directories.insert(resolve_relative_path(path).generic_string());
        }

        void add_file(const std::filesystem::path& path, std::string data)
        {
            files.emplace(resolve_relative_path(path).generic_string(), std::move(data));
        }

        std::filesystem::path get_working_directory() const override
        {
            return working_directory;
        }

        std::filesystem::path get_plugins_directory() const override
        {
            return plugins_directory;
        }

        std::filesystem::path get_logs_directory() const override
        {
            return logs_directory;
        }

        std::vector<std::filesystem::path> get_assets_directories() const override
        {
            std::vector<std::filesystem::path> roots = assets_directories;
            if (!assets_directory.empty())
            {
                bool has_primary = false;
                for (const auto& root : roots)
                {
                    if (root == assets_directory)
                    {
                        has_primary = true;
                        break;
                    }
                }
                if (!has_primary)
                {
                    roots.insert(roots.begin(), assets_directory);
                }
            }
            return roots;
        }

        void add_assets_directory(const std::filesystem::path& path) override
        {
            const auto resolved = resolve_relative_path(path);
            if (assets_directory.empty())
            {
                assets_directory = resolved;
            }
            assets_directories.push_back(resolved);
        }

        std::filesystem::path resolve_asset_path(const std::filesystem::path& path) const override
        {
            if (path.empty())
            {
                return {};
            }
            if (path.is_absolute())
            {
                return path;
            }
            const auto roots = get_assets_directories();
            for (const auto& root : roots)
            {
                if (root.empty())
                {
                    continue;
                }
                const auto candidate = resolve_relative_path(root / path);
                if (exists(candidate))
                {
                    return candidate;
                }
            }
            if (!assets_directory.empty())
            {
                return resolve_relative_path(assets_directory / path);
            }
            return resolve_relative_path(path);
        }

        std::filesystem::path resolve_relative_path(
            const std::filesystem::path& path) const override
        {
            if (path.is_absolute() || working_directory.empty())
            {
                return path;
            }
            return working_directory / path;
        }

        bool exists(const std::filesystem::path& path) const override
        {
            const auto key = resolve_relative_path(path).generic_string();
            return directories.contains(key) || files.contains(key);
        }

        FilePathType get_file_type(const std::filesystem::path& path) const override
        {
            const auto key = resolve_relative_path(path).generic_string();
            if (directories.contains(key))
            {
                return FilePathType::Directory;
            }
            if (files.contains(key))
            {
                return FilePathType::Regular;
            }
            return FilePathType::None;
        }

        std::vector<std::filesystem::path> read_directory(
            const std::filesystem::path& root) const override
        {
            std::vector<std::filesystem::path> entries;
            const std::string prefix = resolve_relative_path(root).generic_string();
            for (const auto& dir : directories)
            {
                if (dir.starts_with(prefix))
                {
                    entries.emplace_back(dir);
                }
            }
            for (const auto& [path, _] : files)
            {
                if (path.starts_with(prefix))
                {
                    entries.emplace_back(path);
                }
            }
            return entries;
        }

        bool create_directory(const std::filesystem::path& path) override
        {
            add_directory(path);
            return true;
        }

        bool create_file(const std::filesystem::path& path) override
        {
            add_file(path, "");
            return true;
        }

        bool read_file(
            const std::filesystem::path& path,
            FileDataFormat,
            std::string& out) const override
        {
            const auto key = resolve_relative_path(path).generic_string();
            auto iterator = files.find(key);
            if (iterator == files.end())
            {
                return false;
            }
            out = iterator->second;
            return true;
        }

        bool write_file(
            const std::filesystem::path& path,
            FileDataFormat,
            const std::string& data) override
        {
            files[resolve_relative_path(path).generic_string()] = data;
            return true;
        }

        bool remove(const std::filesystem::path&) override
        {
            return true;
        }

        bool rename(const std::filesystem::path&, const std::filesystem::path&) override
        {
            return true;
        }

        bool copy(const std::filesystem::path&, const std::filesystem::path&) override
        {
            return true;
        }

        std::unordered_set<std::string> directories;
        std::unordered_map<std::string, std::string> files;
        std::filesystem::path working_directory;
        std::filesystem::path plugins_directory;
        std::filesystem::path logs_directory;
        std::filesystem::path assets_directory;
        std::vector<std::filesystem::path> assets_directories;
    };
}
