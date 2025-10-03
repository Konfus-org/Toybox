#include "Tbx/PCH.h"
#include "Tbx/Plugins/Plugin.h"
#include "Tbx/Debug/Debugging.h"

namespace Tbx
{
    LoadedPlugin::LoadedPlugin(
        const PluginMeta& pluginInfo,
        Ref<EventBus> eventBus)
        : _pluginInfo(pluginInfo)
    {
        Load(eventBus);
    }

    LoadedPlugin::~LoadedPlugin()
    {
        Unload();
    }

    bool LoadedPlugin::IsValid() const noexcept
    {
        return _plugin != nullptr;
    }

    const PluginMeta& LoadedPlugin::GetMeta() const noexcept
    {
        return _pluginInfo;
    }

    void LoadedPlugin::Load(Ref<EventBus> eventBus)
    {
        // Don't load static plugins
        if (_pluginInfo.IsStatic) return;

        const std::string& pluginFullPath = _pluginInfo.Path;
        _library.Load(pluginFullPath);
        if (!_library.IsValid())
        {
            TBX_TRACE_ERROR("Plugin: Failed to load library! Does it exist at: {0}", pluginFullPath);
            return;
        }

        // Get load and unload plugin functions from library
        const auto loadFuncSymbol = _library.GetSymbol(TBX_LOAD_PLUGIN_FN_NAME);
        const auto unloadFuncSymbol = _library.GetSymbol(TBX_UNLOAD_PLUGIN_FN_NAME);
        const auto loadPluginFunc = reinterpret_cast<PluginLoadFn>(loadFuncSymbol);
        if (!loadPluginFunc)
        {
            TBX_TRACE_ERROR("Plugin: Failed to load library because no load library function was found in: {0}, is it calling TBX_REGISTER_PLUGIN?", pluginFullPath);
            _library.Unload();
            return;
        }
        const auto unloadPluginFunc = reinterpret_cast<PluginUnloadFn>(unloadFuncSymbol);
        if (!unloadPluginFunc)
        {
            TBX_TRACE_ERROR("Plugin: No unload library function found in: {0}, is it calling TBX_REGISTER_PLUGIN?", pluginFullPath);
            // Since we already loaded the library we must unload it here to
            // avoid leaking the library handle and associated resources.
            _library.Unload();
            return;
        }

        auto* loadedPlugin = loadPluginFunc(eventBus);
        Ref<IPlugin> sharedLoadedPlugin(loadedPlugin, [unloadPluginFunc](IPlugin* pluginToUnload)
        {
            unloadPluginFunc(pluginToUnload);
        });
        _plugin = sharedLoadedPlugin;

#ifdef TBX_DEBUG
        _library.ListSymbols();
#endif
    }

    void LoadedPlugin::Unload() noexcept
    {
        if (_plugin != nullptr)
        {
            _plugin.reset();
        }

        _library.Unload();
    }
}
