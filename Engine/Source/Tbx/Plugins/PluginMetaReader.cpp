#include "Tbx/PCH.h"
#include "Tbx/Plugins/PluginMetaReader.h"
#include "Tbx/Debug/Tracers.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <map>

namespace Tbx
{
    PluginMetaData PluginMetaReader::Read(const std::string& jsonPath)
    {
        // Open the JSON file
        std::ifstream file(jsonPath);
        if (!file)
        {
            TBX_TRACE_ERROR("PluginMetaReader: Failed to open JSON file: {0}", jsonPath);
            return {};
        }

        // Parse the JSON data into a map
        std::map<std::string, std::vector<std::string>, std::less<>> result;
        nlohmann::json json;
        file >> json;
        for (const auto& entry : json.items())
        {
            if (entry.value().is_array())
            {
                result[entry.key()] = entry.value().get<std::vector<std::string>>();
            }
            if (entry.value().is_string())
            {
                result[entry.key()] = { entry.value().get<std::string>() };
            }
        }

        return result;
    }
}
