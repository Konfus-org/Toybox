#include "Tbx/Core/PCH.h"
#include "Tbx/Core/Plugins/PluginInfo.h"
#include "Tbx/Core/Plugins/PluginMetaReader.h"

void Tbx::PluginInfo::Load(const std::string& location)
{
    auto metaData = PluginMetaReader::Read(location);
    if (metaData.empty()) return;

    _name = metaData["name"];
    _author = metaData["author"];
    _version = metaData["version"];
    _description = metaData["description"];
    _lib = metaData["lib"];
}
