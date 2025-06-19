#include "Tbx/PCH.h"
#include "Tbx/Plugin API/LoadedPlugin.h"
#include "Tbx/Debug/Debugging.h"

namespace Tbx
{
    using PluginLoadFunc = IPlugin*(*)();
    using PluginUnloadFunc = void(*)(IPlugin*);

    LoadedPlugin::LoadedPlugin(const PluginInfo& pluginInfo)
    {
        _pluginInfo = pluginInfo;
        TBX_ASSERT(_pluginInfo.IsValid(), "Cannot load plugin! Invalid plugin info...");

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

    void LoadedPlugin::Restart() const
    {
        _plugin->OnUnload();
        _plugin->OnLoad();
    }

    void LoadedPlugin::Load()
    {
        // Don't load static plugins
        if (_pluginInfo.GetLib().find(".lib") != std::string::npos) return;

        const std::string& pluginFullPath = _pluginInfo.GetLocation() + "\\" + _pluginInfo.GetLib();
        _library.Load(pluginFullPath);
        if (_library.IsValid() == false)
        {
            TBX_ERROR("Failed to load library! Does it exist at: {0}", pluginFullPath);
            return;
        }

        // Get load plugin function from library
        const auto& loadFuncSymbol = _library.GetSymbol("Load");
        if (!loadFuncSymbol)
        {
            TBX_ERROR("Failed to load library because no load library function was found in: {0}, is it calling TBX_REGISTER_PLUGIN?", pluginFullPath);
            _library.Unload();
            return;
        }

        // Ensure we have an unload function
        if (!_library.GetSymbol("Unload"))
        {
            TBX_ERROR("No unload library function found in: {0}, is it calling TBX_REGISTER_PLUGIN?", pluginFullPath);
            return;
        }

        // Load and wrap plugin in shared_ptr with custom destructor
        const auto& loadPluginFunc = static_cast<PluginLoadFunc>(loadFuncSymbol);
        auto* loadedPlugin = loadPluginFunc();
        std::shared_ptr<IPlugin> sharedLoadedPlugin(loadedPlugin, [this](IPlugin* pluginToUnload)
        {
            // Use library to free plugin memory because it owns it
            const auto& unloadFuncSymbol = _library.GetSymbol("Unload");
            if (!unloadFuncSymbol)
            {
                // Couldn't find unload function in plugin library
                const std::string& libraryPath = _library.GetPath();
                TBX_ASSERT(false, "(!!!Likely Memory Leak!!!: Failed to unload the plugin from library: {0}, is it calling TBX_REGISTER_PLUGIN?", libraryPath);
                return;
            }
            const auto& unloadPluginFunc = static_cast<PluginUnloadFunc>(unloadFuncSymbol);
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
            TBX_ASSERT(_plugin.use_count() == 1, "{} Plugin is still in use! Ensure all references are released before unloading!", _pluginInfo.GetName());

            _plugin->OnUnload();
            _plugin.reset();
        }

        _library.Unload();
    }
}
