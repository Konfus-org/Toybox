#pragma once
#include "plugin_ownership_tracker.h"
#include "tbx/interfaces/message_dispatcher.h"
#include "tbx/systems/plugin_api/loaded_plugin.h"
#include "tbx/systems/plugin_api/service_provider.h"

namespace tbx
{
    class PluginUnloader final
    {
      public:
        PluginUnloader() = default;
        ~PluginUnloader() noexcept = default;

      public:
        PluginUnloader(const PluginUnloader&) = delete;
        PluginUnloader& operator=(const PluginUnloader&) = delete;
        PluginUnloader(PluginUnloader&&) = delete;
        PluginUnloader& operator=(PluginUnloader&&) = delete;

      public:
        void detach(
            LoadedPlugins& loaded_plugins,
            ServiceProvider& service_provider,
            IMessageCoordinator* coordinator = nullptr);

        void unload(
            LoadedPlugins& loaded_plugins,
            ServiceProvider& service_provider,
            PluginOwnershipTracker& ownership_tracker,
            IMessageCoordinator* coordinator = nullptr);
    };
}
