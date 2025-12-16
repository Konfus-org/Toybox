#pragma once
#include "tbx/common/string.h"
#include "tbx/tbx_api.h"
#include <filesystem>
#include <format>
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
        FilePath(const String& value);

        FilePathType get_file_type() const;
        String get_extension() const;
        bool is_empty() const;
        bool is_absolute() const;
        bool is_relative() const;
        FilePath get_parent_path() const;
        FilePath get_filename() const;
        FilePath set_extension(const String& extension) const;
        FilePath replace_extension(const String& extension) const;
        FilePath append(const FilePath& component) const;
        FilePath append(const String& component) const;

        operator String() const;
        bool operator==(const FilePath& other) const;
        bool operator!=(const FilePath& other) const;
        FilePath operator+(const FilePath& other) const;
        FilePath& operator+=(const FilePath& other);

      private:
        static std::filesystem::path sanitize_path(const std::filesystem::path& path);
        std::filesystem::path _path;
    };
}
