#include "Tbx/PCH.h"
#include "Tbx/Plugins/Plugin.h"
#include "Tbx/Debug/Debugging.h"

namespace Tbx
{
    Plugin::Plugin(
        const PluginMeta& pluginInfo,
        Ref<EventBus> eventBus)
        : _pluginInfo(pluginInfo)
    {
        Load(eventBus);
    }

    Plugin::~Plugin()
    {
        Unload();
    }

    bool Plugin::IsValid() const
    {
        return _instance != nullptr || _pluginInfo.IsStatic;
    }

    const PluginMeta& Plugin::GetMeta() const
    {
        return _pluginInfo;
    }

    SharedLibrary& Plugin::GetLibrary()
    {
        return _library;
    }

    size_t Plugin::UseCount() const
    {
        return _instance.use_count();
    }

    Ref<void> Plugin::Instance() const
    {
        return _instance;
    }

    void Plugin::Load(Ref<EventBus> eventBus)
    {
        if (_pluginInfo.IsStatic)
        {
            return;
        }

        const std::string& pluginFullPath = _pluginInfo.Path;
        _library.Load(pluginFullPath);
        if (!_library.IsValid())
        {
            TBX_TRACE_ERROR("Plugin: Failed to load library! Does it exist at: {0}", pluginFullPath);
            return;
        }

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
            _library.Unload();
            return;
        }

        auto* loadedPlugin = loadPluginFunc(eventBus);
        if (loadedPlugin == nullptr)
        {
            TBX_TRACE_ERROR("Plugin: Load function returned nullptr for {0}", pluginFullPath);
            _library.Unload();
            return;
        }

        Ref<void> sharedLoadedPlugin(loadedPlugin, [unloadPluginFunc](void* pluginToUnload)
        {
            unloadPluginFunc(pluginToUnload);
        });
        _instance = sharedLoadedPlugin;

#ifdef TBX_DEBUG
        _library.ListSymbols();
#endif
    }

    void Plugin::Unload()
    {
        if (_instance != nullptr)
        {
            _instance.reset();
        }

        if (_library.IsValid())
        {
            _library.Unload();
        }

    }
}
