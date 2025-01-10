#include "TbxPCH.h"
#include "LoadedPlugin.h"
#include "Debug/Debugging.h"

namespace Tbx
{
    LoadedPlugin::LoadedPlugin(const std::string& location)
    {
        Load(location);
    }

    LoadedPlugin::~LoadedPlugin()
    {
        Unload();
    }

    std::weak_ptr<Plugin> Tbx::LoadedPlugin::GetPlugin() const
    {
        return _plugin;
    }

    std::weak_ptr<SharedLibrary> Tbx::LoadedPlugin::GetLibrary() const
    {
        return _library;
    }

    void LoadedPlugin::Load(const std::string& location)
    {
        _library = std::make_shared<SharedLibrary>(location);

#ifdef TBX_DEBUG
        // Uncomment to list symbols
        // library->ListSymbols();
#endif

        // Get load plugin function from library
        using PluginLoadFunc = Plugin*(*)();
        const auto& loadFuncSymbol = _library->GetSymbol("Load");
        if (!loadFuncSymbol)
        {
            const std::string& failureMsg = R"_(Failed to load library because no load library function was found in: {0}, is it calling TBX_REGISTER_PLUGIN?)_";
            TBX_ERROR(failureMsg, location);
            _library.reset();
            return;
        }

        // Ensure we have an unload function
        if (!_library->GetSymbol("Unload"))
        {
            const std::string& failureMsg = R"_(No unload library function found in: {0}, is it calling TBX_REGISTER_PLUGIN?)_";
            TBX_ERROR(failureMsg, location);
            _library.reset();
            return;
        }

        const auto& loadPluginFunc = static_cast<PluginLoadFunc>(loadFuncSymbol);
        auto* loadedPlugin = loadPluginFunc();

        // Wrap plugin in shared_ptr with custom destructor
        std::shared_ptr<Plugin> sharedLoadedPlugin(loadedPlugin, [this](Plugin* pluginToUnload) 
        {
            // Use library to free plugin memory because it owns it
            using PluginUnloadFunc = void(*)(Plugin*);
            const auto& unloadFuncSymbol = _library->GetSymbol("Unload");
            if (!unloadFuncSymbol)
            {
                // Couldn't find unload function in plugin library
                const std::string& libraryPath = _library->GetPath();
                const std::string& failureMsg = R"_(!!!Likely Memory Leak!!!: Failed to unload the plugin from library: {0}, is it calling TBX_REGISTER_PLUGIN?)_";
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
        _library.reset();
    }
}
