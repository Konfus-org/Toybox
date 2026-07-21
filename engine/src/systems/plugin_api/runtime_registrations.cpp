#include "tbx/systems/plugin_api/runtime_registrations.h"
#include <mutex>
#include <unordered_map>
#include <vector>

namespace tbx
{
    // Process-wide list of live per-plugin containers. The engine-core container is NOT listed
    // here: it is reached through engine_core_runtime() and injected first by
    // for_each_plugin_runtime, so only id-bearing plugin containers register — letting
    // active_plugin_runtime() resolve the active plugin id to its container and unload drop it by
    // identity.
    struct RuntimeRegistrationsRegistry
    {
        std::mutex mutex = {};
        std::vector<RuntimeRegistrations*> plugins = {};
        std::unordered_map<Uuid, RuntimeRegistrations*> by_id = {};
    };

    static RuntimeRegistrationsRegistry& runtime_registrations_registry()
    {
        static RuntimeRegistrationsRegistry registry = {};
        return registry;
    }

    RuntimeRegistrations::RuntimeRegistrations(PluginInstanceId id)
        : _id(id)
    {
        // The engine-core container carries no id and stays out of the plugin list; it is the
        // default target and is visited first by the fan-out.
        if (!_id.is_valid())
            return;

        auto& registry = runtime_registrations_registry();
        auto guard = std::lock_guard(registry.mutex);
        registry.by_id[_id] = this;
        registry.plugins.push_back(this);
    }

    RuntimeRegistrations::~RuntimeRegistrations() noexcept
    {
        // Engine-core container: destroyed only at process exit. Do not run unload hooks (they would
        // reach other engine statics whose destruction order relative to this is unspecified) — just
        // let the stores drop. A plugin container, by contrast, is dropped on unload while every
        // engine static is still alive, so it gets its unload hooks (e.g. component instance purge).
        if (!_id.is_valid())
            return;

        {
            auto data_guard = std::lock_guard(_data_mutex);
            for (auto& entry : _data)
                entry.second->on_container_unloading();
        }

        auto& registry = runtime_registrations_registry();
        auto guard = std::lock_guard(registry.mutex);
        registry.by_id.erase(_id);
        std::erase(registry.plugins, this);
    }

    PluginInstanceId RuntimeRegistrations::get_id() const
    {
        return _id;
    }

    void RuntimeRegistrations::add_service(std::type_index service_type)
    {
        if (!_id.is_valid())
            return;

        auto guard = std::lock_guard(_data_mutex);
        _services.push_back(service_type);
    }

    void RuntimeRegistrations::add_asset_directory(const std::filesystem::path& directory)
    {
        if (!_id.is_valid() || directory.empty())
            return;

        auto guard = std::lock_guard(_data_mutex);
        _asset_directories.push_back(directory);
    }

    void RuntimeRegistrations::add_pin(const Handle& handle)
    {
        if (!_id.is_valid() || !handle.is_valid())
            return;

        auto guard = std::lock_guard(_data_mutex);
        _pinned_handles.push_back(handle);
    }

    const std::vector<std::type_index>& RuntimeRegistrations::get_services() const
    {
        return _services;
    }

    const std::vector<std::filesystem::path>& RuntimeRegistrations::get_asset_directories() const
    {
        return _asset_directories;
    }

    const std::vector<Handle>& RuntimeRegistrations::get_pinned_handles() const
    {
        return _pinned_handles;
    }

    RuntimeRegistrations& engine_core_runtime()
    {
        // Function-local static: constructed lazily on first registration, which sidesteps
        // cross-TU static-init ordering — the first built-in registration is what brings it to life.
        static RuntimeRegistrations core = RuntimeRegistrations(PluginInstanceId {});
        return core;
    }

    RuntimeRegistrations& active_plugin_runtime()
    {
        const auto id = get_active_plugin_id();
        if (!id.is_valid())
            return engine_core_runtime();

        auto& registry = runtime_registrations_registry();
        auto guard = std::lock_guard(registry.mutex);
        const auto iterator = registry.by_id.find(id);
        if (iterator == registry.by_id.end())
            return engine_core_runtime();

        return *iterator->second;
    }

    void for_each_plugin_runtime(const std::function<void(RuntimeRegistrations&)>& callback)
    {
        if (!callback)
            return;

        // Engine core first: its built-ins always win a name/type collision against a plugin's.
        callback(engine_core_runtime());

        // Snapshot the plugin list under the lock so a load/unload issued from within the callback
        // cannot invalidate the iteration.
        auto plugins = std::vector<RuntimeRegistrations*> {};
        {
            auto& registry = runtime_registrations_registry();
            auto guard = std::lock_guard(registry.mutex);
            plugins = registry.plugins;
        }

        for (auto* plugin : plugins)
            callback(*plugin);
    }
}
