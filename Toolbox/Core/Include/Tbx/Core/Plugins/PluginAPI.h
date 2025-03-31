#pragma once

#include "Plugin.h"

#ifdef TBX_PLATFORM_WINDOWS
    #define TBX_EXPORT_PLUGIN_ITEM __declspec(dllexport)
#else
    #define TBX_EXPORT_PLUGIN
#endif

// TODO: make plugins event based!
#define TBX_REGISTER_PLUGIN(pluginType) \
    extern "C" TBX_EXPORT_PLUGIN_ITEM pluginType* Load()\
    {\
        return new pluginType();\
    }\
    extern "C" TBX_EXPORT_PLUGIN_ITEM void Unload(pluginType* pluginToUnload)\
    {\
        delete pluginToUnload;\
    }
