#include "tbx/systems/plugin_api/plugin_ownership.h"
#include <atomic>
#include <mutex>
#include <thread>
#include <unordered_map>

namespace tbx
{
    class PluginOwnershipContext final
    {
      public:
        static PluginOwnershipContext& get_instance()
        {
            static PluginOwnershipContext context = {};
            return context;
        }

      public:
        PluginOwnershipContext(const PluginOwnershipContext&) = delete;
        PluginOwnershipContext& operator=(const PluginOwnershipContext&) = delete;
        PluginOwnershipContext(PluginOwnershipContext&&) = delete;
        PluginOwnershipContext& operator=(PluginOwnershipContext&&) = delete;

      public:
        PluginInstanceId allocate()
        {
            const auto seed = _next_plugin_instance_seed.fetch_add(
                1U,
                std::memory_order_relaxed);
            return Uuid::combine(Uuid::generate(), seed);
        }

        PluginInstanceId get_active() const
        {
            auto guard = std::lock_guard(_active_plugin_ids_mutex);
            const auto iterator = _active_plugin_ids.find(std::this_thread::get_id());
            if (iterator == _active_plugin_ids.end())
                return {};

            return iterator->second;
        }

        void set_active(PluginInstanceId plugin_id)
        {
            auto guard = std::lock_guard(_active_plugin_ids_mutex);
            if (!plugin_id.is_valid())
            {
                _active_plugin_ids.erase(std::this_thread::get_id());
                return;
            }

            _active_plugin_ids[std::this_thread::get_id()] = plugin_id;
        }

      private:
        PluginOwnershipContext() = default;
        ~PluginOwnershipContext() noexcept = default;

      private:
        mutable std::mutex _active_plugin_ids_mutex = {};
        std::atomic_uint32_t _next_plugin_instance_seed = 1U;
        std::unordered_map<std::thread::id, PluginInstanceId> _active_plugin_ids = {};
    };

    PluginInstanceId allocate_plugin_instance_id()
    {
        return PluginOwnershipContext::get_instance().allocate();
    }

    PluginInstanceId get_active_plugin_id()
    {
        return PluginOwnershipContext::get_instance().get_active();
    }

    bool has_active_plugin_id()
    {
        return get_active_plugin_id().is_valid();
    }

    void set_active_plugin_id_for_current_thread(PluginInstanceId plugin_id)
    {
        PluginOwnershipContext::get_instance().set_active(plugin_id);
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
