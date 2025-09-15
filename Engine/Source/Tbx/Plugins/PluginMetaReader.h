#pragma once
#include <map>
#include <string>
#include <vector>

namespace Tbx
{
    using PluginMetaData = std::map<std::string, std::vector<std::string>, std::less<>>;

    class PluginMetaReader
    {
    public:
        static PluginMetaData Read(const std::string& jsonPath);
    };
}