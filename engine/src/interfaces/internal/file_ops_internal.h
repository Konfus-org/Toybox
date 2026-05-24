#pragma once
#include "tbx/interfaces/file_ops.h"
#include <filesystem>
#include <string>
#include <system_error>

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

namespace tbx::internal
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

}
