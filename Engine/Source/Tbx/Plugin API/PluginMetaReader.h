#pragma once
#include <map>

namespace Tbx
{
    class PluginMetaReader
    {
    public:
        static std::map<std::string, std::string, std::less<>> Read(const std::string& jsonPath);
    };
}