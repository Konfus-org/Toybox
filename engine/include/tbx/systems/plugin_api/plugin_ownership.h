#pragma once
#include "tbx/tbx_api.h"
#include "tbx/types/uuid.h"

namespace tbx
{
    using PluginInstanceId = Uuid;

    // Allocates a fresh, process-unique id for a loading plugin instance.
    TBX_API PluginInstanceId allocate_plugin_instance_id();
    // The id of the plugin whose ScopedPluginContext is active on the calling thread, or an invalid
    // id when none is (engine code). Registration sites use it to route a registration to the
    // owning plugin's RuntimeRegistrations, or to the engine core when invalid.
    TBX_API PluginInstanceId get_active_plugin_id();
    TBX_API bool has_active_plugin_id();
    TBX_API void set_active_plugin_id_for_current_thread(PluginInstanceId plugin_id);

    /// @brief
    /// Purpose: Marks the calling thread as running the given plugin for the scope's lifetime, so
    /// anything it registers is attributed to that plugin's RuntimeRegistrations. Restores the previous id
    /// on exit (nesting-safe). Pushed around every plugin entry point — create/destroy,
    /// register_services, attach/detach/update/receive_message.
    class TBX_API ScopedPluginContext final
    {
      public:
        explicit ScopedPluginContext(PluginInstanceId plugin_id);
        ~ScopedPluginContext() noexcept;

      public:
        ScopedPluginContext(const ScopedPluginContext&) = delete;
        ScopedPluginContext& operator=(const ScopedPluginContext&) = delete;
        ScopedPluginContext(ScopedPluginContext&&) = delete;
        ScopedPluginContext& operator=(ScopedPluginContext&&) = delete;

      private:
        PluginInstanceId _previous_plugin_id = {};
    };
}
