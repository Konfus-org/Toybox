#include "Tbx/PCH.h"
#include "Tbx/Plugins/LoadedPlugin.h"
#include "Tbx/Debug/Debugging.h"

namespace Tbx
{
    LoadedPlugin::LoadedPlugin(const PluginMeta& pluginInfo, std::weak_ptr<App> app)
    {
        _pluginInfo = pluginInfo;
        Load(app);
    }

    LoadedPlugin::~LoadedPlugin() 
    {
        Unload();
    }

    bool LoadedPlugin::IsValid() const
    {
        return _plugin != nullptr;
    }

    const PluginMeta& LoadedPlugin::GetMeta() const
    {
        return _pluginInfo;
    }

    void LoadedPlugin::Load(std::weak_ptr<App> app)
    {
        // Don't load static plugins
        if (_pluginInfo.IsStatic) return;

        const std::string& pluginFullPath = _pluginInfo.Path;
        _library.Load(pluginFullPath);
        if (!_library.IsValid())
        {
            TBX_TRACE_ERROR("Failed to load library! Does it exist at: {0}", pluginFullPath);
            return;
        }

        // Get load plugin function from library
        const auto loadFuncSymbol = _library.GetSymbol(TBX_LOAD_PLUGIN_FN_NAME);
        if (!loadFuncSymbol)
        {
            TBX_TRACE_ERROR("Failed to load library because no load library function was found in: {0}, is it calling TBX_REGISTER_PLUGIN?", pluginFullPath);
            _library.Unload();
            return;
        }

        const auto unloadFuncSymbol = _library.GetSymbol(TBX_UNLOAD_PLUGIN_FN_NAME);
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
        const auto loadPluginFunc = reinterpret_cast<PluginLoadFn>(loadFuncSymbol);
        const auto unloadPluginFunc = reinterpret_cast<PluginUnloadFn>(unloadFuncSymbol);
        auto* loadedPlugin = loadPluginFunc(app);
        Ref<Plugin> sharedLoadedPlugin(loadedPlugin, [unloadPluginFunc](Plugin* pluginToUnload)
        {
            unloadPluginFunc(pluginToUnload);
        });
        _plugin = sharedLoadedPlugin;

#ifdef TBX_DEBUG
        _library.ListSymbols();
#endif
    }

    void LoadedPlugin::Unload()
    {
        if (_plugin != nullptr)
        {
            _plugin.reset();
        }

        _library.Unload();
    }
}
