#include "tbx/file_system/filesystem_ops.h"
#include <filesystem>
#include <fstream>
#include <system_error>

namespace tbx
{
    class FilesystemOps final : public IFilesystemOps
    {
      public:
        bool exists(const std::filesystem::path& path) const override
        {
            std::error_code ec;
            const bool present = std::filesystem::exists(path, ec);
            return !ec && present;
        }

        bool is_directory(const std::filesystem::path& path) const override
        {
            std::error_code ec;
            const bool dir = std::filesystem::is_directory(path, ec);
            return !ec && dir;
        }

        std::vector<DirectoryEntry> recursive_directory_entries(
            const std::filesystem::path& root) const override
        {
            std::vector<DirectoryEntry> entries;
            std::error_code ec;
            for (std::filesystem::recursive_directory_iterator it(root, ec), end; it != end && !ec;
                 ++it)
            {
                DirectoryEntry entry;
                entry.path = it->path();
                entry.is_regular_file = it->is_regular_file();
                entry.is_directory = it->is_directory();
                entries.push_back(std::move(entry));
            }
            return entries;
        }

        bool read_text_file(const std::filesystem::path& path, std::string& out) const override
        {
            std::ifstream stream(path, std::ios::in | std::ios::binary);
            if (!stream.is_open())
            {
                return false;
            }
            std::string contents(
                (std::istreambuf_iterator<char>(stream)),
                std::istreambuf_iterator<char>());
            out = std::move(contents);
            return true;
        }

        bool remove(const std::filesystem::path& path) override
        {
            std::error_code ec;
            std::filesystem::remove(path, ec);
            return !ec;
        }

        bool rename(const std::filesystem::path& from, const std::filesystem::path& to) override
        {
            std::error_code ec;
            std::filesystem::rename(from, to, ec);
            return !ec;
        }

        bool copy(const std::filesystem::path& from, const std::filesystem::path& to) override
        {
            std::error_code ec;
            std::filesystem::copy_file(
                from,
                to,
                std::filesystem::copy_options::overwrite_existing,
                ec);
            return !ec;
        }
    };

    IFilesystemOps& get_default_filesystem_ops()
    {
        static FilesystemOps ops;
        return ops;
    }
}
