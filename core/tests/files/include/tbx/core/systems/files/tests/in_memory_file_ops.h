#pragma once
#include "tbx/core/interfaces/file_ops.h"
#include <filesystem>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace tbx::tests::file_system
{
    class InMemoryFileOps final : public IFileOps
    {
      public:
        struct FileEntry
        {
            std::string data = {};
            std::filesystem::file_time_type last_write_time =
                std::filesystem::file_time_type::clock::now();
        };

      public:
        InMemoryFileOps(std::filesystem::path working_directory)
            : _working_directory(std::move(working_directory))
        {
        }

      public:
        std::filesystem::path get_working_directory() const override
        {
            return _working_directory;
        }

        std::filesystem::path resolve(const std::filesystem::path& path) const override
        {
            if (path.is_absolute())
                return path.lexically_normal();
            return (_working_directory / path).lexically_normal();
        }

        bool exists(const std::filesystem::path& path) const override
        {
            std::lock_guard<std::mutex> lock(_files_mutex);
            const std::filesystem::path resolved = resolve(path);
            if (_files.contains(resolved))
                return true;

            for (const auto& [file_path, _] : _files)
            {
                if (is_within_directory(file_path, resolved))
                    return true;
            }

            return false;
        }

        FileType get_type(const std::filesystem::path& path) const override
        {
            std::lock_guard<std::mutex> lock(_files_mutex);
            const std::filesystem::path resolved = resolve(path);
            if (_files.contains(resolved))
                return FileType::FILE;

            for (const auto& [file_path, _] : _files)
            {
                if (is_within_directory(file_path, resolved))
                    return FileType::DIRECTORY;
            }

            return FileType::NONE;
        }

        std::filesystem::file_time_type get_last_write_time(
            const std::filesystem::path& path) const override
        {
            std::lock_guard<std::mutex> lock(_files_mutex);
            const auto file = _files.find(resolve(path));
            if (file == _files.end())
                return {};

            return file->second.last_write_time;
        }

        std::vector<std::filesystem::path> read_directory(
            const std::filesystem::path& root) const override
        {
            std::lock_guard<std::mutex> lock(_files_mutex);
            const std::filesystem::path resolved_root = resolve(root);
            std::vector<std::filesystem::path> entries = {};

            for (const auto& [file_path, _] : _files)
            {
                if (is_within_directory(file_path, resolved_root))
                    entries.push_back(file_path);
            }

            return entries;
        }

        bool read_file(
            const std::filesystem::path& path,
            FileDataFormat,
            std::string& out_data) const override
        {
            std::lock_guard<std::mutex> lock(_files_mutex);
            const auto file = _files.find(resolve(path));
            if (file == _files.end())
                return false;

            out_data = file->second.data;
            return true;
        }

        bool write_file(
            const std::filesystem::path& path,
            FileDataFormat,
            const std::string& data) override
        {
            std::lock_guard<std::mutex> lock(_files_mutex);
            FileEntry entry = {};
            entry.data = data;
            _files[resolve(path)] = std::move(entry);
            return true;
        }

      public:
        void erase(const std::filesystem::path& path)
        {
            std::lock_guard<std::mutex> lock(_files_mutex);
            _files.erase(resolve(path));
        }

        void set_binary(std::filesystem::path path, const std::vector<unsigned char>& data)
        {
            std::lock_guard<std::mutex> lock(_files_mutex);
            _files[resolve(path)] =
                FileEntry {
                    .data = std::string(
                        reinterpret_cast<const char*>(data.data()),
                        reinterpret_cast<const char*>(data.data() + data.size())),
                };
        }

        void set_text(std::filesystem::path path, std::string data)
        {
            std::lock_guard<std::mutex> lock(_files_mutex);
            _files[resolve(path)] =
                FileEntry {
                    .data = std::move(data),
                };
        }

        void touch(
            const std::filesystem::path& path,
            std::filesystem::file_time_type last_write_time)
        {
            std::lock_guard<std::mutex> lock(_files_mutex);
            _files[resolve(path)].last_write_time = last_write_time;
        }

        void write_file_entry(
            const std::filesystem::path& path,
            std::string data,
            std::filesystem::file_time_type last_write_time)
        {
            std::lock_guard<std::mutex> lock(_files_mutex);
            _files[resolve(path)] =
                FileEntry {
                    .data = std::move(data),
                    .last_write_time = last_write_time,
                };
        }

      private:
        static bool is_within_directory(
            const std::filesystem::path& file_path,
            const std::filesystem::path& directory_path)
        {
            if (directory_path.empty())
                return false;
            if (file_path == directory_path)
                return false;

            auto directory_iterator = directory_path.begin();
            auto file_iterator = file_path.begin();

            for (; directory_iterator != directory_path.end() && file_iterator != file_path.end();
                 ++directory_iterator, ++file_iterator)
            {
                if (*directory_iterator != *file_iterator)
                    return false;
            }

            return directory_iterator == directory_path.end();
        }

      private:
        std::filesystem::path _working_directory = {};
        mutable std::mutex _files_mutex = {};
        std::unordered_map<std::filesystem::path, FileEntry> _files = {};
    };
}
