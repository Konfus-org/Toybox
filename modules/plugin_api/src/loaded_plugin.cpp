#include "tbx/plugin_api/loaded_plugin.h"
#include <string>

namespace tbx
{
    std::string to_string(const LoadedPlugin& loaded)
    {
        return "Name=" + loaded.meta.name + ", Version=" + loaded.meta.version;
    }
}
