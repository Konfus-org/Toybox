#include "tbx/file_system/filepath.h"
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
        : _path(sanitize_path(std::filesystem::path(static_cast<const std::string&>(value))))
    {
    }

    FilePath::FilePath(std::filesystem::path value)
        : _path(sanitize_path(value))
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

    bool FilePath::empty() const
    {
        return _path.empty();
    }

    FilePath FilePath::parent_path() const
    {
        return FilePath(_path.parent_path());
    }

    FilePath FilePath::filename() const
    {
        return FilePath(_path.filename());
    }

    String FilePath::filename_string() const
    {
        return String(_path.filename().string());
    }

    FilePath FilePath::set_extension(const String& extension) const
    {
        std::filesystem::path updated = _path;
        updated.replace_extension(std::filesystem::path(static_cast<const std::string&>(extension)));
        return FilePath(updated);
    }

    FilePath FilePath::replace_extension(const String& extension) const
    {
        return set_extension(extension);
    }

    FilePath FilePath::append(const String& component) const
    {
        FilePath combined(_path);
        const String sanitized = sanitize_component(component);
        combined._path /= std::filesystem::path(static_cast<const std::string&>(sanitized));
        combined._path = sanitize_path(combined._path);
        return combined;
    }

    const std::filesystem::path& FilePath::std_path() const
    {
        return _path;
    }

    bool FilePath::operator==(const FilePath& other) const
    {
        return _path == other._path;
    }

    bool FilePath::operator!=(const FilePath& other) const
    {
        return !(*this == other);
    }

    FilePath::operator const std::filesystem::path&() const
    {
        return _path;
    }

    String FilePath::sanitize_component(const String& name)
    {
        if (name.empty())
            return "unnamed";

        const String unsupported = "<>:\"/\\|?*";
        std::string sanitized;
        sanitized.reserve(name.size());
        for (char c : std::string_view(name))
        {
            const bool invalid =
                std::string_view(unsupported).find(c) != std::string_view::npos;
            sanitized.push_back(invalid ? '_' : c);
        }
        return String(sanitized);
    }

    std::filesystem::path FilePath::sanitize_path(const std::filesystem::path& path)
    {
        if (path.empty())
            return std::filesystem::path(static_cast<const std::string&>(sanitize_component("")));

        std::filesystem::path cleaned;
        if (!path.root_name().empty())
            cleaned = path.root_name();

        if (!path.root_directory().empty())
            cleaned /= path.root_directory();

        bool has_component = false;
        for (const auto& part : path.relative_path())
        {
            const String sanitized_part = sanitize_component(part.string());
            cleaned /= std::filesystem::path(static_cast<const std::string&>(sanitized_part));
            has_component = true;
        }

        if (!has_component && cleaned.empty())
            cleaned = std::filesystem::path(static_cast<const std::string&>(sanitize_component(path.string())));

        return cleaned;
    }
}
