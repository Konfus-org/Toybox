#pragma once
#include "Tbx/DllExport.h"
#include <memory>
#include "Tbx/Memory/Refs/Refs.h"

namespace Tbx
{
    // Forward declaration for below macro
    class App;

    /// <summary>
    /// Anything in the tbx lib that is intented to be a plugin should inherit from this interface!
    /// Plugins defined inside plugin libraries can also directly inherit from this if they only need to hook into things on load/unload.
    /// Otherwise its recommended to implement one of the TBX interfaces that inherit from this directly (defined below this interface in PluginInterfaces.h).
    /// </summary>
    class EXPORT Plugin
    {
    public:
        virtual ~Plugin() = default;
        virtual void OnLoad() {}
        virtual void OnUnload() {}
    };
}

/// <summary>
/// Macro to register a plugin to the TBX plugin system.
/// Is required for TBX to be able to load the plugin.
/// </summary>
#define TBX_REGISTER_PLUGIN(pluginType) \
    extern "C" EXPORT pluginType* Load(const Tbx::WeakRef<Tbx::App>& app)\
    {\
        return new pluginType(app);\
    }\
    extern "C" EXPORT void Unload(pluginType* pluginToUnload)\
    {\
        delete pluginToUnload;\
    }