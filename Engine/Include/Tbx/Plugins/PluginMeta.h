#pragma once
#include "Tbx/DllExport.h"
#include "Tbx/Debug/IPrintable.h"
#include <string>
#include <vector>

namespace Tbx
{
    struct TBX_EXPORT PluginMeta : public IPrintable
    {
        std::string Name = "";
        std::string Path = "";
        std::string Author = "";
        std::string Version = "";
        std::string Description = "";
        std::vector<std::string> Dependencies = {};

        std::string ToString() const override;
    };
}
