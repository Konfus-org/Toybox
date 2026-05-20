#include "tbx/interfaces/file_ops.h"
#include "tbx/interfaces/internal/file_ops_internal.h"
#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#include <vector>
#if defined(TBX_PLATFORM_WINDOWS)
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #include <windows.h>
#elif defined(TBX_PLATFORM_LINUX)
    #include <unistd.h>
#elif defined(TBX_PLATFORM_MACOS)
    #include <mach-o/dyld.h>
#endif

namespace tbx
{
    FileOperator::FileOperator(std::filesystem::path working_directory)
        : _working_directory(working_directory.lexically_normal())
    {
        if (_working_directory.empty())
        {
            _working_directory = internal::get_default_working_directory();
        }
    }

    std::filesystem::path FileOperator::get_working_directory() const
    {
        return _working_directory;
    }

    std::filesystem::path FileOperator::resolve(const std::filesystem::path& path) const
    {
        return internal::resolve_with_working(_working_directory, path);
    }

    bool FileOperator::is_valid(const std::filesystem::path& path) const
    {
        return !resolve(path).empty();
    }

    bool FileOperator::exists(const std::filesystem::path& path) const
    {
        const std::filesystem::path resolved = resolve(path);
        std::error_code ec;
        return std::filesystem::exists(resolved, ec) && !ec;
    }

    FileType FileOperator::get_type(const std::filesystem::path& path) const
    {
        const std::filesystem::path resolved = resolve(path);
        std::error_code ec;
        const auto status = std::filesystem::status(resolved, ec);
        if (ec)
            return FileType::NONE;
        if (status.type() == std::filesystem::file_type::regular)
            return FileType::FILE;
        if (status.type() == std::filesystem::file_type::directory)
            return FileType::DIRECTORY;
        return FileType::OTHER;
    }

    std::filesystem::file_time_type FileOperator::get_last_write_time(
        const std::filesystem::path& path) const
    {
        const std::filesystem::path resolved = resolve(path);
        std::error_code ec;
        const auto last_write_time = std::filesystem::last_write_time(resolved, ec);
        if (ec)
            return {};
        return last_write_time;
    }

    std::vector<std::filesystem::path> FileOperator::read_directory(
        const std::filesystem::path& root) const
    {
        std::vector<std::filesystem::path> entries;
        const std::filesystem::path resolved_root = resolve(root);
        std::error_code ec;
        for (std::filesystem::recursive_directory_iterator it(resolved_root, ec), end;
             it != end && !ec;
             ++it)
        {
            entries.emplace_back(it->path());
        }
        return entries;
    }

    bool FileOperator::create(const std::filesystem::path& path)
    {
        const std::filesystem::path resolved = resolve(path);
        if (resolved.empty())
            return false;
        const FileType type = internal::get_create_type_for_path(resolved);
        std::error_code ec;

        switch (type)
        {
            case FileType::DIRECTORY:
                std::filesystem::create_directories(resolved, ec);
                return !ec;
            case FileType::FILE:
            {
                auto std_path = std::filesystem::path(resolved);
                if (std_path.has_parent_path())
                    std::filesystem::create_directories(std_path.parent_path(), ec);
                if (ec)
                    return false;
                std::ofstream stream(std_path, std::ios::out | std::ios::trunc);
                return stream.is_open();
            }
            case FileType::NONE:
            case FileType::OTHER:
                return false;
        }

        return false;
    }

    bool FileOperator::read_file(
        const std::filesystem::path& path,
        FileDataFormat format,
        std::string& out) const
    {
        const std::filesystem::path resolved = resolve(path);
        const bool binary = format == FileDataFormat::BINARY;
        std::ios_base::openmode mode = std::ios::in;
        if (binary)
            mode |= std::ios::binary;

        auto std_path = std::filesystem::path(resolved);
        auto stream = std::ifstream(std_path, mode);
        if (!stream.is_open())
            return false;

        auto contents =
            std::string(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());

        // Strip UTF-8 BOM for text mode; binary payloads are left untouched.
        if (!binary && contents.size() >= 3)
        {
            const unsigned char bom0 = static_cast<unsigned char>(contents[0]);
            const unsigned char bom1 = static_cast<unsigned char>(contents[1]);
            const unsigned char bom2 = static_cast<unsigned char>(contents[2]);
            if (bom0 == 0xEF && bom1 == 0xBB && bom2 == 0xBF)
                contents.erase(0, 3);
        }

        out = contents;
        return true;
    }

    bool FileOperator::write_file(
        const std::filesystem::path& path,
        FileDataFormat format,
        const std::string& data)
    {
        const std::filesystem::path resolved = resolve(path);
        const bool binary = format == FileDataFormat::BINARY;
        std::ios_base::openmode mode = std::ios::out | std::ios::trunc;
        if (binary)
            mode |= std::ios::binary;

        auto std_path = std::filesystem::path(resolved);
        std::ofstream stream(std_path, mode);
        if (!stream.is_open())
            return false;

        stream.write(data.data(), static_cast<std::streamsize>(data.size()));
        return stream.good();
    }

    bool FileOperator::remove(const std::filesystem::path& path)
    {
        const std::filesystem::path resolved = resolve(path);
        std::error_code ec;
        auto std_path = std::filesystem::path(resolved);
        std::filesystem::remove(std_path, ec);
        return !ec;
    }

    bool FileOperator::rename(const std::filesystem::path& from, const std::filesystem::path& to)
    {
        if (!copy(from, to))
            return false;
        return remove(from);
    }

    bool FileOperator::copy(const std::filesystem::path& from, const std::filesystem::path& to)
    {
        const std::filesystem::path resolved_from = resolve(from);
        const std::filesystem::path resolved_to = resolve(to);
        auto std_path_from = std::filesystem::path(resolved_from);
        auto std_path_to = std::filesystem::path(resolved_to);
        std::error_code ec;
        std::filesystem::copy_file(
            std_path_from,
            std_path_to,
            std::filesystem::copy_options::overwrite_existing,
            ec);
        return !ec;
    }

    std::filesystem::path FileOperator::rotate(
        const std::filesystem::path& directory,
        std::string_view base_name,
        std::string_view extension,
        int max_history)
    {
        const std::string sanitized = std::filesystem::path(base_name).filename().string();
        const std::filesystem::path resolved_root = resolve(directory);
        if (!create(resolved_root))
            return {};

        if (max_history < 1)
            return internal::make_rotated_path(resolved_root, sanitized, extension, 0);

        for (int index = max_history; index >= 1; index--)
        {
            const auto from =
                internal::make_rotated_path(resolved_root, sanitized, extension, index - 1);
            const auto to = internal::make_rotated_path(resolved_root, sanitized, extension, index);

            if (!exists(from))
                continue;
            if (exists(to))
                remove(to);
            if (rename(from, to))
                continue;
            if (copy(from, to))
                remove(from);
        }

        return internal::make_rotated_path(resolved_root, sanitized, extension, 0);
    }
}
