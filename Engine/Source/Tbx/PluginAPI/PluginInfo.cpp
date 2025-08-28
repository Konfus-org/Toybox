#include "Tbx/PCH.h"
#include "Tbx/PluginAPI/PluginInfo.h"
#include "Tbx/PluginAPI/PluginMetaReader.h"
#include <sstream>

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

        _name = metaData["name"][0];
        _author = metaData["author"][0];
        _version = metaData["version"][0];
        _description = metaData["description"][0];
        _lib = metaData["lib"][0];
        _dependencies = metaData["dependencies"];
        _implementedTypes = metaData["type"];
    }
}
