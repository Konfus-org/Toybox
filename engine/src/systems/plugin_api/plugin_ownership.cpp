#include "tbx/systems/plugin_api/plugin_ownership.h"
#include <atomic>

namespace tbx
{
    namespace detail
    {
        static thread_local PluginInstanceId g_active_plugin_id = PluginInstanceId{};
        static std::atomic_uint32_t g_next_plugin_instance_seed = 1U;
    }

    PluginInstanceId allocate_plugin_instance_id()
    {
        const auto seed = detail::g_next_plugin_instance_seed.fetch_add(1U, std::memory_order_relaxed);
        return Uuid::combine(Uuid::generate(), seed);
    }

    PluginInstanceId get_active_plugin_id()
    {
        return detail::g_active_plugin_id;
    }

    bool has_active_plugin_id()
    {
        return get_active_plugin_id().is_valid();
    }

    void set_active_plugin_id_for_current_thread(PluginInstanceId plugin_id)
    {
        detail::g_active_plugin_id = plugin_id;
    }

    ScopedPluginContext::ScopedPluginContext(PluginInstanceId plugin_id)
        : _previous_plugin_id(get_active_plugin_id())
    {
        set_active_plugin_id_for_current_thread(plugin_id);
    }

    ScopedPluginContext::~ScopedPluginContext() noexcept
    {
        set_active_plugin_id_for_current_thread(_previous_plugin_id);
    }
}
