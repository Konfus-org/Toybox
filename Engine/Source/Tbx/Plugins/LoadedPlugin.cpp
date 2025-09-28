#include "Tbx/PCH.h"
#include "Tbx/Plugins/PluginServerRecord.h"
#include "Tbx/Debug/Debugging.h"

namespace Tbx
{
    PluginServerRecord::PluginServerRecord(const PluginMeta& pluginInfo, std::weak_ptr<App> app)
    {
        _pluginInfo = pluginInfo;
        Load(app);
    }

    PluginServerRecord::~PluginServerRecord()
    {
        Unload();
    }

    bool PluginServerRecord::IsValid() const
    {
        return _plugin != nullptr;
    }

    const PluginMeta& PluginServerRecord::GetMeta() const
    {
        return _pluginInfo;
    }

    void PluginServerRecord::Load(std::weak_ptr<App> app)
    {
        // Don't load static plugins
        if (_pluginInfo.IsStatic) return;

        const std::string& pluginFullPath = _pluginInfo.Path;
        _library.Load(pluginFullPath);
        if (!_library.IsValid())
        {
            TBX_TRACE_ERROR("PluginServerRecord: Failed to load library! Does it exist at: {0}", pluginFullPath);
            return;
        }

        // Get load and unload plugin functions from library
        const auto loadFuncSymbol = _library.GetSymbol(TBX_LOAD_PLUGIN_FN_NAME);
        const auto unloadFuncSymbol = _library.GetSymbol(TBX_UNLOAD_PLUGIN_FN_NAME);
        const auto loadPluginFunc = reinterpret_cast<PluginLoadFn>(loadFuncSymbol);
        if (!loadPluginFunc)
        {
            TBX_TRACE_ERROR("PluginServerRecord: Failed to load library because no load library function was found in: {0}, is it calling TBX_REGISTER_PLUGIN?", pluginFullPath);
            _library.Unload();
            return;
        }
        const auto unloadPluginFunc = reinterpret_cast<PluginUnloadFn>(unloadFuncSymbol);
        if (!unloadPluginFunc)
        {
            TBX_TRACE_ERROR("PluginServerRecord: No unload library function found in: {0}, is it calling TBX_REGISTER_PLUGIN?", pluginFullPath);
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

    void PluginServerRecord::Unload()
    {
        if (_plugin != nullptr)
        {
            _plugin.reset();
        }

        _library.Unload();
    }
}
