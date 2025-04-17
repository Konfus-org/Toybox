#include "Tbx/Runtime/PCH.h"
#include "Tbx/Runtime/Runtime.h"

namespace Tbx
{
    AppStatus Run(std::shared_ptr<App> app)
    {
        try
        {
            // Launch first
            app->Launch();

            // Load layer plugins
            auto layerPlugins = PluginServer::GetPlugins<Layer>();
            for (const auto& layerPlugin : layerPlugins)
            {
                app->PushLayer(layerPlugin);
            }

            // Then run the app!
            while (app->GetStatus() == AppStatus::Running)
            {
                app->Update();
            }

            // Cleanup
            app->Close();
        }
        catch (const std::exception& ex)
        {
            TBX_ERROR("{0}", ex.what());
            return AppStatus::Error;
        }

        return app->GetStatus();
    }
}