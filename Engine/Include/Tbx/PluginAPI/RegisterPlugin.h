#pragma once
#include "Tbx/PluginAPI/PluginInterfaces.h"
#include "Tbx/DllExport.h"

/// <summary>
/// Macro to register a plugin to the TBX plugin system.
/// Is required for TBX to be able to load the plugin.
/// </summary>
#define TBX_REGISTER_PLUGIN(pluginType) \
    extern "C" EXPORT pluginType* Load()\
    {\
        return new pluginType();\
    }\
    extern "C" EXPORT void Unload(pluginType* pluginToUnload)\
    {\
        delete pluginToUnload;\
    }
