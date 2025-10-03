#pragma once
#include "Tbx/DllExport.h"
#include "Tbx/Plugins/PluginMeta.h"
#include <string>
#include <vector>

namespace Tbx
{
    class TBX_EXPORT PluginFinder
    {
    public:
        PluginFinder(
            std::string searchDirectory,
            std::vector<std::string> requestedPlugins = {});

        std::vector<PluginMeta> Result() &&;

    private:
        std::vector<PluginMeta> Discover(const std::string& searchDirectory) const;

    private:
        std::vector<std::string> _requested;
        std::vector<PluginMeta> _discovered;
    };
}
