#pragma once
#include "tbx/common/string.h"
#include "tbx/tbx_api.h"
#include <filesystem>
#include <string_view>

namespace tbx
{
    enum class FilePathType
    {
        None,
        Regular,
        Directory,
        Other
    };

    struct TBX_API FilePath
    {
        FilePath() = default;
        FilePath(const char* value);
        FilePath(String_view value);
        FilePath(const String& value);
        FilePath(std::filesystem::path value);

        FilePathType get_file_type() const;
        String get_extension() const;
        bool empty() const;
        FilePath parent_path() const;
        FilePath filename() const;
        String filename_string() const;
        FilePath set_extension(const String& extension) const;
        FilePath replace_extension(const String& extension) const;
        FilePath append(String_view component) const;

        const std::filesystem::path& std_path() const;

        bool operator==(const FilePath& other) const;
        bool operator!=(const FilePath& other) const;
        operator const std::filesystem::path&() const;

      private:
        static std::filesystem::path sanitize_path(const std::filesystem::path& path);
        static String sanitize_component(String_view name);
        std::filesystem::path _path;
    };
}
