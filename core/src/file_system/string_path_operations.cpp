#include "tbx/file_system/string_path_operations.h"
#include <cctype>
#include <filesystem>
#include <string>
#include <string_view>

namespace tbx
{
    std::string sanitize_for_file_name_usage(const std::string_view value)
    {
        std::string result;
        result.reserve(value.size());

        for (unsigned char ch : value)
        {
            if (std::isalnum(ch) != 0 || ch == '-' || ch == '_')
            {
                result.push_back(static_cast<char>(ch));
            }
            else
            {
                result.push_back('_');
            }
        }

        if (result.empty())
        {
            result = "unnamed";
        }

        return result;
    }

    std::string filename_only(const std::string_view path)
    {
        if (path.empty())
        {
            return {};
        }

        return std::filesystem::path(path).filename().string();
    }
}
