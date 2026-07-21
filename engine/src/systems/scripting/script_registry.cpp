#include "tbx/systems/scripting/script_registry.h"
#include "tbx/systems/plugin_api/runtime_registrations.h"
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>

namespace tbx
{
    // One plugin's (or the engine core's) script registrations, owned by that plugin's
    // RuntimeRegistrations.
    // Keyed by the script's stable type name; the values are plain function pointers, so dropping the
    // container on unload trivially destroys them (no module-hosted std::function manager runs). Live
    // script instances are already dropped first by the ScriptSystem's PluginUnloadingEvent handler.
    struct ScriptRegistrations final : RuntimeRegistrationsData
    {
        std::mutex mutex = {};
        std::unordered_map<std::string, ScriptRegistration> entries = {};
    };

    void register_script_entry(
        RuntimeRegistrations& owner,
        std::string type_name,
        ScriptRegistration entry)
    {
        auto& data = owner.get_data<ScriptRegistrations>();
        auto guard = std::lock_guard(data.mutex);
        data.entries.insert_or_assign(std::move(type_name), entry);
    }

    const ScriptRegistration* get_script_registration(std::string_view type_name)
    {
        // Fan out engine core first: the first container that carries the name wins. The returned
        // pointer stays valid until its container is dropped (unordered_map keeps element addresses
        // stable across inserts/rehashes; entries are only erased on unload / shutdown).
        const ScriptRegistration* found = nullptr;
        const auto key = std::string(type_name);
        for_each_plugin_runtime(
            [&found, &key](RuntimeRegistrations& runtime)
            {
                if (found)
                    return;

                auto* data = runtime.try_get_data<ScriptRegistrations>();
                if (!data)
                    return;

                auto guard = std::lock_guard(data->mutex);
                const auto iterator = data->entries.find(key);
                if (iterator != data->entries.end())
                    found = &iterator->second;
            });
        return found;
    }

    void clear_script_registrations()
    {
        // Shutdown: drop every container's script function pointers while their modules are still
        // mapped, so no pointer into an about-to-unload module lingers.
        for_each_plugin_runtime(
            [](RuntimeRegistrations& runtime)
            {
                auto* data = runtime.try_get_data<ScriptRegistrations>();
                if (!data)
                    return;

                auto guard = std::lock_guard(data->mutex);
                data->entries.clear();
            });
    }
}
