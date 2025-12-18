#include "tbx/files/filesystem.h"
#include <filesystem>
#include <fstream>
#include <system_error>
#include <utility>

namespace tbx
{
    static FilePath resolve_with_working(const FilePath& working_directory, const FilePath& path)
    {
        if (path.is_empty())
            return FilePath();
        if (path.is_absolute() || working_directory.is_empty())
            return path;
        return FilePath(working_directory) + FilePath(path);
    }

    static FilePath choose_directory(
        const FilePath& working_directory,
        const FilePath& candidate_path,
        const String& default_path)
    {
        if (!candidate_path.is_empty())
            return resolve_with_working(working_directory, candidate_path);
        return FilePath(working_directory) + FilePath(default_path);
    }

    FileSystem::FileSystem(
        const FilePath& working_directory,
        const FilePath& plugins_directory,
        const FilePath& logs_directory,
        const FilePath& assets_directory)
        : _working_directory(working_directory)
        , _plugins_directory(plugins_directory)
        , _logs_directory(logs_directory)
        , _assets_directory(assets_directory)
    {
        if (_working_directory.is_empty())
        {
            std::error_code ec;
            _working_directory = FilePath(std::filesystem::current_path(ec).string().c_str());
            if (ec)
                _working_directory = FilePath();
        }

        // Default subdirectories always hang off the working directory unless overridden.
        _plugins_directory = choose_directory(_working_directory, _plugins_directory, "");
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
        String resolved_str = resolved;
        std::error_code ec;
        return std::filesystem::exists(resolved_str.get_cstr(), ec) && !ec;
    }

    FilePathType FileSystem::get_file_type(const FilePath& path) const
    {
        const FilePath resolved = resolve_relative_path(path);
        String resolved_str = resolved;
        std::error_code ec;
        const auto status = std::filesystem::status(resolved_str.get_cstr(), ec);
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
        String resolved_str = resolved_root;
        std::error_code ec;
        for (std::filesystem::recursive_directory_iterator it(resolved_str.get_cstr(), ec), end;
             it != end && !ec;
             ++it)
        {
            entries.emplace(it->path().string().c_str());
        }
        return entries;
    }

    bool FileSystem::create_directory(const FilePath& path)
    {
        const FilePath resolved = resolve_relative_path(path);
        String resolved_str = resolved;
        std::error_code ec;
        std::filesystem::create_directories(resolved_str.get_cstr(), ec);
        return !ec;
    }

    bool FileSystem::create_file(const FilePath& path)
    {
        const FilePath resolved = resolve_relative_path(path);
        String resolved_str = resolved;
        std::error_code ec;
        auto std_path = std::filesystem::path(resolved_str.get_cstr());
        if (std_path.has_parent_path())
            std::filesystem::create_directories(std_path.parent_path(), ec);
        if (ec)
            return false;
        std::ofstream stream(std_path, std::ios::out | std::ios::trunc);
        return stream.is_open();
    }

    bool FileSystem::read_file(const FilePath& path, FileDataFormat format, String& out) const
    {
        const FilePath resolved = resolve_relative_path(path);
        String resolved_str = resolved;
        const bool binary = format == FileDataFormat::Binary;
        std::ios_base::openmode mode = std::ios::in;
        if (binary)
            mode |= std::ios::binary;

        auto std_path = std::filesystem::path(resolved_str.get_cstr());
        auto stream = std::ifstream(std_path, mode);
        if (!stream.is_open())
            return false;

        auto contents =
            String(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());

        // Strip UTF-8 BOM for text mode; binary payloads are left untouched.
        if (!binary && contents.size() >= 3)
        {
            const unsigned char bom0 = static_cast<unsigned char>(contents[0]);
            const unsigned char bom1 = static_cast<unsigned char>(contents[1]);
            const unsigned char bom2 = static_cast<unsigned char>(contents[2]);
            if (bom0 == 0xEF && bom1 == 0xBB && bom2 == 0xBF)
                contents.remove(0, 3);
        }

        out = contents;
        return true;
    }

    bool FileSystem::write_file(const FilePath& path, FileDataFormat format, const String& data)
    {
        const FilePath resolved = resolve_relative_path(path);
        String resolved_str = resolved;
        const bool binary = format == FileDataFormat::Binary;
        std::ios_base::openmode mode = std::ios::out | std::ios::trunc;
        if (binary)
            mode |= std::ios::binary;

        auto std_path = std::filesystem::path(resolved_str.get_cstr());
        std::ofstream stream(std_path, mode);
        if (!stream.is_open())
            return false;

        stream.write(data.get_cstr(), static_cast<std::streamsize>(data.size()));
        return stream.good();
    }

    bool FileSystem::remove(const FilePath& path)
    {
        const FilePath resolved = resolve_relative_path(path);
        String resolved_str = resolved;
        std::error_code ec;
        auto std_path = std::filesystem::path(resolved_str.get_cstr());
        std::filesystem::remove(std_path, ec);
        return !ec;
    }

    bool FileSystem::rename(const FilePath& from, const FilePath& to)
    {
        if (!copy(from, to))
            return false;
        return remove(from);
    }

    bool FileSystem::copy(const FilePath& from, const FilePath& to)
    {
        const FilePath resolved_from = resolve_relative_path(from);
        String resolved_str_from = resolved_from;
        const FilePath resolved_to = resolve_relative_path(to);
        String resolved_str_to = resolved_to;
        auto std_path_from = std::filesystem::path(resolved_str_from.get_cstr());
        auto std_path_to = std::filesystem::path(resolved_str_to.get_cstr());
        std::error_code ec;
        std::filesystem::copy_file(
            std_path_from,
            std_path_to,
            std::filesystem::copy_options::overwrite_existing,
            ec);
        return !ec;
    }
}
