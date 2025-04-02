#include "Tbx/Core/PCH.h"
#include "Tbx/Core/Plugins/LoadedPlugin.h"
#include <Tbx/Core/Debug/DebugAPI.h>

namespace Tbx
{
    LoadedPlugin::LoadedPlugin(const std::string& pluginFolderPath, const std::string& pluginFileName)
    {
        Load(pluginFolderPath, pluginFileName);
    }

    LoadedPlugin::~LoadedPlugin() 
    { 
        Unload(); 
    }

    bool LoadedPlugin::IsValid() const
    {
        return _plugin != nullptr;
    }

    const PluginInfo& LoadedPlugin::GetPluginInfo() const
    {
        return _pluginInfo;
    }

    std::shared_ptr<IPlugin> LoadedPlugin::GetPlugin() const
    {
        return _plugin;
    }

    void LoadedPlugin::Load(const std::string& pluginFolderPath, const std::string& pluginFileName)
    {
        // Load plugin metadata
        const auto& pluginMetadataPath = pluginFolderPath + "\\" + pluginFileName;
        _pluginInfo.Load(pluginMetadataPath);

        // Check to see if this is an application being loaded
        if (_pluginInfo.IsValid() == false)
        {
            const auto& appMetadataPath = pluginFolderPath + "\\" + "app.meta";
            _pluginInfo.Load(appMetadataPath);
        }

        const std::string& pluginFullPath = pluginFolderPath + "\\" + _pluginInfo.GetLib();
        _library.Load(pluginFullPath);
        if (_library.IsValid() == false)
        {
            const std::string& failureMsg = "Failed to load library! Does it exist at: {0}";
            TBX_ERROR(failureMsg, pluginFullPath);
        }

        // TODO: have a verbose mode!
#ifdef TBX_DEBUG
        // Uncomment to list symbols
        // library->ListSymbols();
#endif

        // Get load plugin function from library
        using PluginLoadFunc = IPlugin*(*)();
        const auto& loadFuncSymbol = _library.GetSymbol("Load");
        if (!loadFuncSymbol)
        {
            const std::string& failureMsg = "Failed to load library because no load library function was found in: {0}, is it calling TBX_REGISTER_PLUGIN?";
            TBX_ERROR(failureMsg, pluginFullPath);
            _library.Unload();
            return;
        }

        // Ensure we have an unload function
        if (!_library.GetSymbol("Unload"))
        {
            const std::string& failureMsg = "No unload library function found in: {0}, is it calling TBX_REGISTER_PLUGIN?";
            TBX_ERROR(failureMsg, pluginFullPath);
            return;
        }

        const auto& loadPluginFunc = static_cast<PluginLoadFunc>(loadFuncSymbol);
        auto* loadedPlugin = loadPluginFunc();

        // Wrap plugin in shared_ptr with custom destructor
        std::shared_ptr<IPlugin> sharedLoadedPlugin(loadedPlugin, [this](IPlugin* pluginToUnload)
        {
            // Use library to free plugin memory because it owns it
            using PluginUnloadFunc = void(*)(IPlugin*);
            const auto& unloadFuncSymbol = _library.GetSymbol("Unload");
            if (!unloadFuncSymbol)
            {
                // Couldn't find unload function in plugin library
                const std::string& libraryPath = _library.GetPath();
                const std::string& failureMsg = "(!!!Likely Memory Leak!!!: Failed to unload the plugin from library: {0}, is it calling TBX_REGISTER_PLUGIN?";
                TBX_ASSERT(false, failureMsg, libraryPath);
                return;
            }
            const auto& unloadPluginFunc = static_cast<PluginUnloadFunc>(unloadFuncSymbol);
            unloadPluginFunc(pluginToUnload);
        });

        // Set and init plugin
        _plugin = sharedLoadedPlugin;
        _plugin->OnLoad();
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
