#include "Tbx/PCH.h"
#include "Tbx/Plugin API/PluginInfo.h"
#include "Tbx/Plugin API/PluginMetaReader.h"

namespace Tbx
{
    PluginInfo::PluginInfo(const std::string& pathToPluginFolder, const std::string& pluginFileName)
    {
        _pathToFolder = pathToPluginFolder;
        Load(pathToPluginFolder + "\\" + pluginFileName);
    }

    std::string PluginInfo::ToString() const
    {
        return std::format("Name: {}\nAuthor: {}\nVersion: {}\nDescription: {}", _name, _author, _version, _description);
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
