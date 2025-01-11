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

        TBX_API static std::shared_ptr<IPlugin> GetPlugin(const std::string_view& name);
        TBX_API static std::vector<std::shared_ptr<LoadedPlugin>> GetLoadedPlugins();

        template <typename T>
        TBX_API static std::shared_ptr<T> GetPlugin();

    private:
        static std::vector<std::shared_ptr<LoadedPlugin>> _loadedPlugins;
    };

    // Explicit instantiations for plugin types
    template std::shared_ptr<IInputHandler> TBX_API PluginServer::GetPlugin<IInputHandler>();
    template std::shared_ptr<IWindow> TBX_API PluginServer::GetPlugin<IWindow>();
    template std::shared_ptr<IRenderer> TBX_API PluginServer::GetPlugin<IRenderer>();
    template std::shared_ptr<ILogger> TBX_API PluginServer::GetPlugin<ILogger>();
}

