#include "Tbx/PCH.h"
#include "Tbx/PluginAPI/LoadedPlugin.h"
#include "Tbx/Debug/Debugging.h"

namespace Tbx
{
    using PluginLoadFunc = IPlugin*(*)();
    using PluginUnloadFunc = void(*)(IPlugin*);

    LoadedPlugin::LoadedPlugin(const PluginInfo& pluginInfo)
    {
        _pluginInfo = pluginInfo;
        Load();
    }

    LoadedPlugin::~LoadedPlugin() 
    {
        Unload();
    }

    bool LoadedPlugin::IsValid() const
    {
        return _plugin != nullptr;
    }

    const PluginInfo& LoadedPlugin::GetInfo() const
    {
        return _pluginInfo;
    }

    void LoadedPlugin::Reload()
    {
        Unload();
        Load();
    }

    void LoadedPlugin::Load()
    {
        // Don't load static plugins
        if (_pluginInfo.GetLib().find(".lib") != std::string::npos) return;

        const std::string& pluginFullPath = _pluginInfo.GetPathToLib();
        _library.Load(pluginFullPath);
        if (!_library.IsValid())
        {
            TBX_TRACE_ERROR("Failed to load library! Does it exist at: {0}", pluginFullPath);
            return;
        }

        // Get load plugin function from library
        const auto loadFuncSymbol = _library.GetSymbol("Load");
        if (!loadFuncSymbol)
        {
            TBX_TRACE_ERROR("Failed to load library because no load library function was found in: {0}, is it calling TBX_REGISTER_PLUGIN?", pluginFullPath);
            _library.Unload();
            return;
        }

        const auto unloadFuncSymbol = _library.GetSymbol("Unload");
        if (!unloadFuncSymbol)
        {
            TBX_TRACE_ERROR("No unload library function found in: {0}, is it calling TBX_REGISTER_PLUGIN?", pluginFullPath);
            // Since we already loaded the library we must unload it here to
            // avoid leaking the library handle and associated resources.
            _library.Unload();
            return;
        }

        // Load and wrap plugin in shared_ptr with custom destructor
        // GCC does not allow static_cast from void* to function pointer
        // (it's also not guaranteed to be valid by the standard).  Use
        // reinterpret_cast instead which is the correct way to cast the
        // symbol returned from the dynamic library loader to a function
        // pointer.
        const auto loadPluginFunc = reinterpret_cast<PluginLoadFunc>(loadFuncSymbol);
        const auto unloadPluginFunc = reinterpret_cast<PluginUnloadFunc>(unloadFuncSymbol);
        auto* loadedPlugin = loadPluginFunc();
        std::shared_ptr<IPlugin> sharedLoadedPlugin(loadedPlugin, [unloadPluginFunc](IPlugin* pluginToUnload)
        {
            unloadPluginFunc(pluginToUnload);
        });

        // Set and init plugin
        _plugin = sharedLoadedPlugin;
        _plugin->OnLoad();

#ifdef TBX_DEBUG
        _library.ListSymbols();
#endif
    }

    void LoadedPlugin::Unload()
    {
        if (_plugin != nullptr)
        {
            _plugin->OnUnload();
            _plugin.reset();
        }

        _library.Unload();
    }
}
