#pragma once
#include "tbx/systems/assets/serialization.h"
#include "tbx/systems/files/json.h"
#include "tbx/tbx_api.h"
#include "tbx/types/typedefs.h"
#include "tbx/utils/result.h"
#include <string>
#include <string_view>
#include <utility>

namespace tbx
{
    class RuntimeRegistrations;
    class ScriptContext;

    /// @brief
    /// Purpose: The script-specific runtime callbacks for one compiled script type — apply per-binding
    /// overrides onto an instance, and bind runtime services/references.
    /// @details
    /// Lives in the engine (a normally-loaded module, never shadow-copied) so the registry has a single
    /// instance shared by the module that compiles the scripts and the backend that reads them; a plugin
    /// could not host it because the plugin loader shadow-copies plugin DLLs into per-load duplicates,
    /// and the authoring module (which does not link the backend plugin) could never reach that copy.
    /// The callbacks are PLAIN function pointers, not std::function: when a scripts plugin hot-reloads,
    /// its old entry is purged (see below); a function pointer is trivially destroyed, whereas a
    /// std::function would run a move/destroy manager that lives in the just-unloaded module and crash.
    struct ScriptRegistration
    {
        Result (*apply_overrides)(const Json&, void*) = nullptr;
        void (*bind_runtime)(void*, ScriptContext&) = nullptr;
    };

    // Registrations are keyed by the script's stable TYPE NAME (a heap string), never by std::type_index:
    // a type_index holds a type_info* into the owning module, so after a scripts plugin unloads its old
    // entries' keys would dangle and the next map rehash would read freed memory. A name key is always
    // valid, and a reload re-registers the same name after the old entry is purged.
    // Records a script registration into the owning module's RuntimeRegistrations. Dropping that
    // container on unload releases the entry, so its function pointers never dangle after the module
    // unmaps — no separate tracking is needed.
    TBX_API void register_script_entry(
        RuntimeRegistrations& owner,
        std::string type_name,
        ScriptRegistration entry);
    // Looks a script up by type name, fanning out across the engine core and every loaded plugin
    // (engine core first).
    TBX_API const ScriptRegistration* get_script_registration(std::string_view type_name);
    // Clears every container's script registrations. Used at shutdown before the owning modules unload.
    TBX_API void clear_script_registrations();

    /// @brief
    /// Registers a compiled script type: as a normal JSON asset body (so the AssetManager can load its
    /// prototype, marked is_script for the editor), plus its override/bind callbacks in the script
    /// registry. Emitted by codegen for every [[tbx::register_script]] type.
    /// @details
    /// The apply/bind helpers are non-type template parameters so the type-erasing thunks below are
    /// non-capturing lambdas, which decay to plain function pointers stored in the registry.
    template <
        typename TScript,
        Result (*ApplyOverrides)(const Json&, TScript&),
        void (*BindRuntime)(TScript&, ScriptContext&)>
    bool register_script_type(RuntimeRegistrations& owner, uint32 version)
    {
        auto entry = make_asset_type_registration<TScript>(version);
        entry.read_body = [](std::string_view data, void* asset)
        {
            return read_json_asset_body(data, *static_cast<TScript*>(asset));
        };
        entry.write_body = [](const void* asset, std::string& output)
        {
            return write_json_asset_body(*static_cast<const TScript*>(asset), output);
        };
        entry.is_script = true;
        register_asset_type_entry(owner, std::move(entry));

        register_script_entry(
            owner,
            std::string(serialization_type_name(static_cast<const TScript*>(nullptr))),
            ScriptRegistration {
                .apply_overrides = [](const Json& json, void* asset) -> Result
                {
                    return ApplyOverrides(json, *static_cast<TScript*>(asset));
                },
                .bind_runtime = [](void* asset, ScriptContext& context)
                {
                    BindRuntime(*static_cast<TScript*>(asset), context);
                },
            });
        return true;
    }
}
