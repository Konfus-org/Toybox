#pragma once
#include "Tbx/Debug/Debugging.h"
#include <string>
#include <filesystem>

#ifndef TBX_WORKING_ROOT_DIR
    #define TBX_WORKING_ROOT_DIR "./"
#endif

namespace Tbx::FileSystem
{
    inline std::string GetWorkingDirectory()
    {
        return TBX_WORKING_ROOT_DIR;
    }

    inline std::string GetRelativePath(const std::filesystem::path& absolutePath)
    {
        std::error_code ec;
        auto relative = std::filesystem::relative(absolutePath, GetWorkingDirectory(), ec);
        if (ec)
        {
            TBX_TRACE_WARNING("FileSystem: unable to compute relative path for {}: {}", absolutePath.string(), ec.message());
            relative = absolutePath.filename();
        }

        return relative.generic_string();
    }

    inline std::string NormalizePath(const std::filesystem::path& path)
    {
        auto normalized = path.generic_string();
        std::transform(normalized.begin(), normalized.end(), normalized.begin(), [](unsigned char ch)
        {
            return static_cast<char>(std::tolower(ch));
        });

        return normalized;
    }
}