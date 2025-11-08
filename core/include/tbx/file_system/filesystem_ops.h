#pragma once
#pragma once
#include "tbx/tbx_api.h"
#include <filesystem>
#include <string>
#include <vector>

namespace tbx
{
    struct TBX_API DirectoryEntry
    {
        std::filesystem::path path;
        bool is_regular_file = false;
        bool is_directory = false;
    };

    class TBX_API IFilesystemOps
    {
      public:
        virtual ~IFilesystemOps() = default;
        virtual bool exists(const std::filesystem::path& path) const = 0;
        virtual bool is_directory(const std::filesystem::path& path) const = 0;
        virtual std::vector<DirectoryEntry>
            recursive_directory_entries(const std::filesystem::path& root) const = 0;
        virtual bool read_text_file(const std::filesystem::path& path, std::string& out) const = 0;
        virtual bool remove(const std::filesystem::path& path) = 0;
        virtual bool rename(const std::filesystem::path& from, const std::filesystem::path& to) = 0;
        virtual bool copy(const std::filesystem::path& from, const std::filesystem::path& to) = 0;
    };

    IFilesystemOps& get_default_filesystem_ops();
}
