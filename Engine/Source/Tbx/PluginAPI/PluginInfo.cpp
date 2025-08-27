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

        _name = metaData["name"];
        _author = metaData["author"];
        _version = metaData["version"];
        _description = metaData["description"];
        _lib = metaData["lib"];

        const auto parseDeps = [this](const std::string& deps)
        {
            if (deps.empty()) return;
            std::stringstream ss(deps);
            std::string dep;
            while (std::getline(ss, dep, ','))
            {
                if (!dep.empty()) _dependencies.push_back(dep);
            }
        };

        parseDeps(metaData["dependencies"]);
        parseDeps(metaData["type_dependencies"]);
    }
}
