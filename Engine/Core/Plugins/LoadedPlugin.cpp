#include "TbxPCH.h"
#include "LoadedPlugin.h"
#include "Debug/DebugAPI.h"
#include <fstream>

namespace Tbx
{
    void LoadedPlugin::Load(const std::string& location)
    {
        _library.Load(location);
        if (_library.IsValid() == false)
        {
            const std::string& failureMsg = "Failed to load library! Does it exist at: {0}";
            TBX_ERROR(failureMsg, location);
        }

        // TODO: also load plugin metadata/info


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
            TBX_ERROR(failureMsg, location);
            _library.Unload();
            return;
        }

        // Ensure we have an unload function
        if (!_library.GetSymbol("Unload"))
        {
            const std::string& failureMsg = "No unload library function found in: {0}, is it calling TBX_REGISTER_PLUGIN?";
            TBX_ERROR(failureMsg, location);
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
