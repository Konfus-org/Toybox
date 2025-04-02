#pragma once

#include "Plugin.h"

#define TBX_REGISTER_PLUGIN(pluginType) \
    extern "C" __declspec(dllexport) pluginType* Load()\
    {\
        return new pluginType();\
    }\
    extern "C" __declspec(dllexport) void Unload(pluginType* pluginToUnload)\
    {\
        delete pluginToUnload;\
    }
