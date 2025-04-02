#pragma once
#include "Tbx/Runtime/Plugin Server/LoadedPlugin.h"
#include <Tbx/Core/DllExport.h>
#include <memory>
#include <string>
#include <vector>

namespace Tbx
{
    class PluginServer
    {
    public:
        // TODO: implement a priority system for plugin loading, i.e. logging should ALWAYS be loaded first...
        EXPORT static void LoadPlugins(const std::string& pathToPlugins);
        EXPORT static void ReloadPlugins(const std::string& pathToPlugins);
        EXPORT static void Shutdown();

        EXPORT static void RegisterPlugin(std::shared_ptr<LoadedPlugin> plugin);
        EXPORT static std::vector<std::shared_ptr<LoadedPlugin>> GetLoadedPlugins();
        EXPORT static std::shared_ptr<IPlugin> GetPlugin(const std::string_view& name);

        template <typename T>
        EXPORT static std::shared_ptr<T> GetPlugin()
        {
            const auto& loadedPlugins = GetLoadedPlugins();
            for (const auto& loadedPlug : loadedPlugins)
            {
                const auto& plug = loadedPlug->GetPlugin();
                const auto& castedPlug = std::dynamic_pointer_cast<Plugin<T>>(plug);
                if (castedPlug)
                {
                    return castedPlug->GetImplementation();
                }
            }

            return nullptr;
        }

        template <typename T>
        EXPORT static std::vector<std::shared_ptr<T>> GetPlugins()
        {
            std::vector<std::shared_ptr<T>> plugins;
            const auto& loadedPlugins = GetLoadedPlugins();
            for (const auto& loadedPlug : loadedPlugins)
            {
                const auto& plug = loadedPlug->GetPlugin();
                const auto& castedPlug = std::dynamic_pointer_cast<Plugin<T>>(plug);
                if (castedPlug)
                {
                    plugins.push_back(castedPlug->GetImplementation());
                }
            }

            return plugins;
        }

    private:
        static std::vector<std::shared_ptr<LoadedPlugin>> _loadedPlugins;
    };
}

