#include "TbxPCH.h"
#include "PluginMetaReader.h"
#include <nlohmann/json.hpp>
#include <fstream>

namespace Tbx
{
    std::map<std::string, std::string, std::less<>> PluginMetaReader::Read(const std::string& jsonPath)
    {
        // Open the JSON file
        std::ifstream file(jsonPath);
        if (!file)
        {
            TBX_ERROR("Failed to open JSON file: {0}", jsonPath);
            return {};
        }

        // Parse the JSON data into a map
        std::map<std::string, std::string, std::less<>> result;
        nlohmann::json json;
        file >> json;
        for (const auto& entry : json.items())
        {
            result[entry.key()] = entry.value().get<std::string>();
        }

        return result;
    }
}
