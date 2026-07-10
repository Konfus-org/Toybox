#include "tbx/systems/scripting/script_registry.h"
#include "tbx/systems/plugin_api/plugin_ownership_tracking.h"
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>

namespace tbx
{
    static std::unordered_map<std::string, ScriptRegistration>& script_registrations()
    {
        static auto g_registrations = std::unordered_map<std::string, ScriptRegistration> {};
        return g_registrations;
    }

    static std::mutex& script_registry_mutex()
    {
        static auto g_mutex = std::mutex {};
        return g_mutex;
    }

    void register_script_entry(std::string type_name, ScriptRegistration entry)
    {
        {
            auto guard = std::lock_guard(script_registry_mutex());
            script_registrations().insert_or_assign(type_name, entry);
        }

        // Tag the entry with the currently-loading plugin so it is purged in lock-step with the asset-type
        // registration created in the same register_script_type call when that module unloads. Takes the
        // lock separately: the tracker has its own mutex and must not nest under ours.
        track_plugin_owned_script_registration(type_name);
    }

    const ScriptRegistration* get_script_registration(std::string_view type_name)
    {
        auto guard = std::lock_guard(script_registry_mutex());
        const auto iterator = script_registrations().find(std::string(type_name));
        if (iterator == script_registrations().end())
            return nullptr;

        return &iterator->second;
    }

    void unregister_script_entry(std::string_view type_name)
    {
        if (type_name.empty())
            return;

        auto guard = std::lock_guard(script_registry_mutex());
        script_registrations().erase(std::string(type_name));
    }

    void clear_script_registrations()
    {
        auto guard = std::lock_guard(script_registry_mutex());
        script_registrations().clear();
    }
}
