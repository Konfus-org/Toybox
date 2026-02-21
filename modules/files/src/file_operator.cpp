#include "tbx/files/file_ops.h"
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
    static std::filesystem::path get_executable_directory()
    {
#if defined(TBX_PLATFORM_WINDOWS)
        std::wstring buffer = {};
        buffer.resize(1024);

        for (;;)
        {
            const DWORD size =
                GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
            if (size == 0)
            {
                return {};
            }

            if (size < (buffer.size() - 1))
            {
                buffer.resize(size);
                break;
            }

            if (buffer.size() >= 32768)
            {
                return {};
            }

            buffer.resize(buffer.size() * 2);
        }

        return std::filesystem::path(buffer).parent_path().lexically_normal();
#elif defined(TBX_PLATFORM_LINUX)
        std::array<char, 4096> buffer = {};
        const std::int64_t size = readlink("/proc/self/exe", buffer.data(), buffer.size() - 1);
        if (size <= 0)
        {
            return {};
        }

        buffer[static_cast<std::size_t>(size)] = '\0';
        return std::filesystem::path(buffer.data()).parent_path().lexically_normal();
#elif defined(TBX_PLATFORM_MACOS)
        std::uint32_t required_size = 0;
        _NSGetExecutablePath(nullptr, &required_size);
        if (required_size == 0)
        {
            return {};
        }

        std::vector<char> buffer = {};
        buffer.resize(required_size);
        if (_NSGetExecutablePath(buffer.data(), &required_size) != 0)
        {
            return {};
        }

        return std::filesystem::path(buffer.data()).parent_path().lexically_normal();
#else
        return {};
#endif
    }

    static std::filesystem::path get_current_path()
    {
        std::error_code ec;
        auto current = std::filesystem::current_path(ec);
        if (ec)
            return {};
        return current;
    }

    static std::filesystem::path get_default_working_directory()
    {
        const auto executable_directory = get_executable_directory();
        if (!executable_directory.empty())
        {
            return executable_directory;
        }

        return get_current_path();
    }

    static std::filesystem::path resolve_with_working(
        const std::filesystem::path& working_directory,
        const std::filesystem::path& path)
    {
        if (path.empty())
            return {};
        if (path.is_absolute() || working_directory.empty())
            return path.lexically_normal();
        return (working_directory / path).lexically_normal();
    }

    static std::filesystem::path make_rotated_path(
        const std::filesystem::path& directory,
        std::string_view base_name,
        std::string_view extension,
        int index)
    {
        const std::string stem = std::string(base_name);
        const std::string suffix = index <= 0 ? std::string() : "_" + std::to_string(index);
        const std::string ext = extension.empty() ? std::string() : std::string(extension);
        return directory / (stem + suffix + ext);
    }

    static FileType get_create_type_for_path(const std::filesystem::path& path)
    {
        if (path.empty())
            return FileType::NONE;
        if (path.has_extension())
            return FileType::FILE;
        return FileType::DIRECTORY;
    }

    FileOperator::FileOperator(std::filesystem::path working_directory)
        : _working_directory(working_directory.lexically_normal())
    {
        if (_working_directory.empty())
        {
            _working_directory = get_default_working_directory();
        }
    }

    std::filesystem::path FileOperator::get_working_directory() const
    {
        return _working_directory;
    }

    std::filesystem::path FileOperator::resolve(const std::filesystem::path& path) const
    {
        return resolve_with_working(_working_directory, path);
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
        const FileType type = get_create_type_for_path(resolved);
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
            return make_rotated_path(resolved_root, sanitized, extension, 0);

        for (int index = max_history; index >= 1; index--)
        {
            const auto from = make_rotated_path(resolved_root, sanitized, extension, index - 1);
            const auto to = make_rotated_path(resolved_root, sanitized, extension, index);

            if (!exists(from))
                continue;
            if (exists(to))
                remove(to);
            if (rename(from, to))
                continue;
            if (copy(from, to))
                remove(from);
        }

        return make_rotated_path(resolved_root, sanitized, extension, 0);
    }
}
