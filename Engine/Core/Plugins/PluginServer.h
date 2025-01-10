#pragma once
#include "TbxAPI.h"
#include "LoadedPlugin.h"
#include "Input/IInputHandler.h"
#include "Windowing/IWindow.h"
#include "Debug/ILogger.h"

namespace Tbx
{
    class PluginServer
    {
    public:
        // TODO: implement a priority system for plugin loading, i.e. logging should ALWAYS be loaded first...
        TBX_API static void LoadPlugins(const std::string& pathToPlugins);
        TBX_API static void Shutdown();

        //TBX_API static std::weak_ptr<Plugin> GetPlugin(const std::string_view& name);
        TBX_API static std::vector<std::weak_ptr<Plugin>> GetPlugins();

        template <typename T>
        TBX_API static std::weak_ptr<FactoryPlugin<T>> GetPlugin();

    private:
        static std::vector<std::shared_ptr<LoadedPlugin>> _loadedPlugins;
    };

    // Explicit instantiations for plugin types
    template std::weak_ptr<FactoryPlugin<IInputHandler>> TBX_API PluginServer::GetPlugin<IInputHandler>();
    template std::weak_ptr<FactoryPlugin<IWindow>> TBX_API PluginServer::GetPlugin<IWindow>();
    template std::weak_ptr<FactoryPlugin<IRenderer>> TBX_API PluginServer::GetPlugin<IRenderer>();
    template std::weak_ptr<FactoryPlugin<ILogger>> TBX_API PluginServer::GetPlugin<ILogger>();
}

