#include "Tbx/PCH.h"
#include "Tbx/Plugins/PluginMeta.h"
#include <format>

namespace Tbx
{
    std::string PluginMeta::ToString() const
    {
        return std::format("Name: {}\nAuthor: {}\nVersion: {}\nDescription: {}", Name, Author, Version, Description);
    }
}
