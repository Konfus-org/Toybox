#pragma once
#include <map>
#include <string>
#include <vector>

namespace Tbx
{
    using PluginMeta = std::map<std::string, std::vector<std::string>, std::less<>>;

    class PluginMetaReader
    {
    public:
        static PluginMeta Read(const std::string& jsonPath);
    };
}