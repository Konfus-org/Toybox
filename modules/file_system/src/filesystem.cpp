#include "tbx/file_system/filesystem.h"
#include <filesystem>
#include <fstream>
#include <system_error>
#include <utility>

namespace tbx
{
    static FilePath resolve_with_working(const FilePath& working_directory, const FilePath& path)
    {
        if (path.empty())
            return FilePath();
        if (path.std_path().is_absolute() || working_directory.empty())
            return path;
        return FilePath(working_directory.std_path() / path.std_path());
    }

    static FilePath choose_directory(
        const FilePath& working_directory,
        const FilePath& candidate,
        const String& folder_name)
    {
        if (!candidate.empty())
            return resolve_with_working(working_directory, candidate);

        if (working_directory.empty())
            return FilePath(folder_name);

        return FilePath(working_directory.std_path() / folder_name.std_str());
    }

    FileSystem::FileSystem(
        FilePath working_directory,
        FilePath plugins_directory,
        FilePath logs_directory,
        FilePath assets_directory)
        : _working_directory(std::move(working_directory))
        , _plugins_directory(std::move(plugins_directory))
        , _logs_directory(std::move(logs_directory))
        , _assets_directory(std::move(assets_directory))
    {
        if (_working_directory.empty())
        {
            std::error_code ec;
            _working_directory = FilePath(std::filesystem::current_path(ec));
            if (ec)
                _working_directory = FilePath();
        }

        // Default subdirectories always hang off the working directory unless overridden.
        _plugins_directory = choose_directory(_working_directory, _plugins_directory, "plugins");
        _logs_directory = choose_directory(_working_directory, _logs_directory, "logs");
        _assets_directory = choose_directory(_working_directory, _assets_directory, "assets");
    }

    FilePath FileSystem::get_working_directory() const
    {
        return _working_directory;
    }

    FilePath FileSystem::get_plugins_directory() const
    {
        return _plugins_directory;
    }

    FilePath FileSystem::get_logs_directory() const
    {
        return _logs_directory;
    }

    FilePath FileSystem::get_assets_directory() const
    {
        return _assets_directory;
    }

    FilePath FileSystem::resolve_relative_path(const FilePath& path) const
    {
        return resolve_with_working(_working_directory, path);
    }

    bool FileSystem::exists(const FilePath& path) const
    {
        const FilePath resolved = resolve_relative_path(path);
        std::error_code ec;
        return std::filesystem::exists(resolved.std_path(), ec) && !ec;
    }

    FilePathType FileSystem::get_file_type(const FilePath& path) const
    {
        const FilePath resolved = resolve_relative_path(path);
        std::error_code ec;
        const auto status = std::filesystem::status(resolved.std_path(), ec);
        if (ec)
            return FilePathType::None;

        if (status.type() == std::filesystem::file_type::regular)
            return FilePathType::Regular;
        if (status.type() == std::filesystem::file_type::directory)
            return FilePathType::Directory;
        return FilePathType::Other;
    }

    List<FilePath> FileSystem::read_directory(const FilePath& root) const
    {
        List<FilePath> entries;
        const FilePath resolved_root = resolve_relative_path(root);
        std::error_code ec;
        for (std::filesystem::recursive_directory_iterator it(resolved_root.std_path(), ec), end;
             it != end && !ec;
             ++it)
        {
            entries.emplace(it->path());
        }
        return entries;
    }

    bool FileSystem::create_directory(const FilePath& path)
    {
        const FilePath resolved = resolve_relative_path(path);
        std::error_code ec;
        std::filesystem::create_directories(resolved.std_path(), ec);
        return !ec;
    }

    bool FileSystem::create_file(const FilePath& path)
    {
        const FilePath resolved = resolve_relative_path(path);
        std::error_code ec;
        if (!resolved.std_path().parent_path().empty())
            std::filesystem::create_directories(resolved.std_path().parent_path(), ec);
        if (ec)
            return false;

        std::ofstream stream(resolved.std_path(), std::ios::out | std::ios::trunc);
        return stream.is_open();
    }

    bool FileSystem::read_file(const FilePath& path, String& out, FileDataFormat format) const
    {
        const FilePath resolved = resolve_relative_path(path);
        const bool binary = format == FileDataFormat::Binary;
        std::ios_base::openmode mode = std::ios::in;
        if (binary)
            mode |= std::ios::binary;

        std::ifstream stream(resolved.std_path(), mode);
        if (!stream.is_open())
            return false;

        String contents(
            (std::istreambuf_iterator<char>(stream)),
            std::istreambuf_iterator<char>());

        // Strip UTF-8 BOM for text mode; binary payloads are left untouched.
        if (!binary && contents.size() >= 3)
        {
            const std::string& storage = contents.std_str();
            const unsigned char bom0 = static_cast<unsigned char>(storage[0]);
            const unsigned char bom1 = static_cast<unsigned char>(storage[1]);
            const unsigned char bom2 = static_cast<unsigned char>(storage[2]);
            if (bom0 == 0xEF && bom1 == 0xBB && bom2 == 0xBF)
                contents.erase(0, 3);
        }

        out = contents;
        return true;
    }

    bool FileSystem::write_file(const FilePath& path, const String& data, FileDataFormat format)
    {
        const FilePath resolved = resolve_relative_path(path);
        const bool binary = format == FileDataFormat::Binary;
        std::ios_base::openmode mode = std::ios::out | std::ios::trunc;
        if (binary)
            mode |= std::ios::binary;

        std::ofstream stream(resolved.std_path(), mode);
        if (!stream.is_open())
            return false;

        stream.write(data.c_str(), static_cast<std::streamsize>(data.size()));
        return stream.good();
    }

    bool FileSystem::remove(const FilePath& path)
    {
        const FilePath resolved = resolve_relative_path(path);
        std::error_code ec;
        std::filesystem::remove(resolved.std_path(), ec);
        return !ec;
    }

    bool FileSystem::rename(const FilePath& from, const FilePath& to)
    {
        const FilePath resolved_from = resolve_relative_path(from);
        const FilePath resolved_to = resolve_relative_path(to);
        std::error_code ec;
        std::filesystem::rename(resolved_from.std_path(), resolved_to.std_path(), ec);
        return !ec;
    }

    bool FileSystem::copy(const FilePath& from, const FilePath& to)
    {
        const FilePath resolved_from = resolve_relative_path(from);
        const FilePath resolved_to = resolve_relative_path(to);
        std::error_code ec;
        std::filesystem::copy_file(
            resolved_from.std_path(),
            resolved_to.std_path(),
            std::filesystem::copy_options::overwrite_existing,
            ec);
        return !ec;
    }
}
