#pragma once
#include "tbx/tbx_api.h"
#include "tbx/types/uuid.h"

namespace tbx
{
    using PluginInstanceId = Uuid;

    TBX_API PluginInstanceId allocate_plugin_instance_id();
    TBX_API PluginInstanceId get_active_plugin_id();
    TBX_API bool has_active_plugin_id();
    TBX_API void set_active_plugin_id_for_current_thread(PluginInstanceId plugin_id);

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
