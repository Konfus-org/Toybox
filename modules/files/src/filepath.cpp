#include "tbx/files/filepath.h"
#include <filesystem>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

namespace tbx
{
    FilePath::FilePath(const char* value)
        : _path(sanitize_path(value ? std::filesystem::path(value) : std::filesystem::path()))
    {
    }

    FilePath::FilePath(const String& value)
        : _path(sanitize_path(std::filesystem::path(value.get_cstr())))
    {
    }

    FilePathType FilePath::get_file_type() const
    {
        if (_path.empty())
            return FilePathType::None;

        std::error_code ec;
        const auto status = std::filesystem::status(_path, ec);
        if (ec)
            return FilePathType::None;

        if (status.type() == std::filesystem::file_type::regular)
            return FilePathType::Regular;
        if (status.type() == std::filesystem::file_type::directory)
            return FilePathType::Directory;
        return FilePathType::Other;
    }

    String FilePath::get_extension() const
    {
        return String(_path.extension().string());
    }

    bool FilePath::is_empty() const
    {
        return _path.empty();
    }

    bool FilePath::is_absolute() const
    {
        return _path.is_absolute();
    }

    bool FilePath::is_relative() const
    {
        return _path.is_relative();
    }

    FilePath FilePath::get_parent_path() const
    {
        return FilePath(_path.parent_path().string().c_str());
    }

    FilePath FilePath::get_filename() const
    {
        return FilePath(_path.filename().string().c_str());
    }

    FilePath FilePath::set_extension(const String& extension) const
    {
        std::filesystem::path updated = _path;
        updated.replace_extension(std::filesystem::path(extension.get_cstr()));
        return FilePath(updated.string().c_str());
    }

    FilePath FilePath::replace_extension(const String& extension) const
    {
        return set_extension(extension);
    }

    FilePath FilePath::append(const FilePath& component) const
    {
        String component_str = component;
        return append(component_str);
    }

    FilePath FilePath::append(const String& component) const
    {
        FilePath combined(_path.string().c_str());
        combined._path /= std::filesystem::path(component.get_cstr());
        combined._path = sanitize_path(combined._path);
        return combined;
    }

    bool FilePath::operator==(const FilePath& other) const
    {
        return _path == other._path;
    }

    bool FilePath::operator!=(const FilePath& other) const
    {
        return !(*this == other);
    }

    FilePath FilePath::operator+(const FilePath& other) const
    {
        return append(other);
    }

    FilePath& FilePath::operator+=(const FilePath& other)
    {
        String other_str = other;
        _path /= std::filesystem::path(other_str.get_cstr());
        return *this;
    }

    FilePath::operator String() const
    {
        return String(_path.string());
    }

    std::filesystem::path FilePath::sanitize_path(const std::filesystem::path& path)
    {
        if (path.empty())
            return "";

        const List<char> unsupported = {'<', '>', '\"', '|', '?', '*'};
        auto path_str = String(path.string()).remove("./").remove("\\.").replace(unsupported, '_');

        return std::filesystem::path(path_str.get_cstr());
    }
}
