#include "TbxPCH.h"
#include "LoadedPlugin.h"
#include "Debug/DebugAPI.h"

namespace Tbx
{
    void LoadedPlugin::Load(const std::string& folderPath, const std::string& pluginDllFileName)
    {
        // Load plugin metadata
        const auto& pluginMetadataPath = folderPath + "\\" + "plugin.meta";
        _pluginInfo.Load(pluginMetadataPath);
        if (_pluginInfo.IsValid() == false)
        {
            const std::string& failureMsg = "Failed to load plugin metadata! Does it exist at: {0}";
            TBX_ERROR(failureMsg, pluginMetadataPath);
        }

        const std::string& pluginFullPath = folderPath + "\\" + pluginDllFileName;
        _library.Load(pluginFullPath);
        if (_library.IsValid() == false)
        {
            const std::string& failureMsg = "Failed to load library! Does it exist at: {0}";
            TBX_ERROR(failureMsg, pluginFullPath);
        }

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
        _plugin->OnUnload();
        _plugin.reset();
        _library.Unload();
    }
}
