#pragma once
#include "IPlugin.h"

/// <summary>
/// Macro to register a plugin to the TBX plugin system.
/// Is required for TBX to be able to load the plugin.
/// </summary>
#define TBX_REGISTER_PLUGIN(pluginType) \
    extern "C" __declspec(dllexport) pluginType* Load()\
    {\
        return new pluginType();\
    }\
    extern "C" __declspec(dllexport) void Unload(pluginType* pluginToUnload)\
    {\
        delete pluginToUnload;\
    }
