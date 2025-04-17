#include "Tbx/Loader/Loader.h"
#include "Tbx/Core/Plugins/PluginServer.h"
#include <Tbx/Core/Debug/DebugAPI.h>
#include <Tbx/Core/Events/EventCoordinator.h>
#include <Tbx/Runtime/Layers/Layer.h>
#include <Tbx/Runtime/Input/Input.h>
#include <Tbx/Runtime/App/App.h>
#include <chrono>
#include <sstream>
#include <format>

namespace Tbx
{
    std::shared_ptr<App> Load(const std::string& pathToPlugins)
    {
        try
        {
            PluginServer::LoadPlugins(pathToPlugins);

            auto apps = PluginServer::GetPlugins<App>();

            TBX_ASSERT(apps.size() != 0,
                "Toybox needs an app defined to run, have you setup an app correctly?"
                "The app library should be loaded as a tbx plugin to allow for hot reloading and you should have a host application to call 'Tbx::Run'.");
            TBX_ASSERT(!(apps.size() > 1), "Toybox only supports one app at a time!");

            auto app = apps[0];
            TBX_VALIDATE_PTR(app, "Could not load app! Is the TBX_PATH_TO_PLUGINS defined correctly and is the dll containing the app being build there?");

            TBX_INFO("Loaded app: {0}", app->GetName());

            return app;
        }
        catch (const std::exception& ex)
        {
            TBX_ERROR("{0}", ex.what());
            return nullptr;
        }
    }
}