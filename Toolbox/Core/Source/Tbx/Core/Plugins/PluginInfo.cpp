#include "Tbx/Core/PCH.h"
#include "Tbx/Core/Plugins/PluginInfo.h"
#include "Tbx/Core/Plugins/PluginMetaReader.h"

namespace Tbx
{
    PluginInfo::PluginInfo(const std::string& pathToPluginFolder, const std::string& pluginFileName)
    {
        _pathToFolder = pathToPluginFolder;
        Load(pathToPluginFolder + "\\" + pluginFileName);
    }

    void PluginInfo::Load(const std::string& pathToPluginFile)
    {
        auto metaData = PluginMetaReader::Read(pathToPluginFile);
        if (metaData.empty()) return;

        _name = metaData["name"];
        _author = metaData["author"];
        _version = metaData["version"];
        _description = metaData["description"];
        _lib = metaData["lib"];

        const auto& prio = metaData["priority"];
        if (prio.empty()) return;

        _priority = std::stoi(prio);
    }
}
