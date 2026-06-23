#pragma once
#include "tbx/cpp_scripting/cpp_scripting_api.h"
#include "tbx/systems/assets/serialization.h"
#include "tbx/systems/files/json.h"
#include "tbx/types/typedefs.h"
#include "tbx/utils/result.h"
#include <string>
#include <string_view>
#include <utility>

namespace tbx
{
    class ScriptContext;

    /// @brief
    /// Purpose: The script-specific runtime callbacks for one compiled C++ script type — apply
    /// per-binding overrides onto an instance, and bind runtime services/references.
    /// @details
    /// Lives in the cpp_scripting runtime lib (a normally-loaded module, never shadow-copied) so the
    /// registry has a single instance shared by the script-owning module and the backend. The callbacks
    /// are PLAIN function pointers, not std::function: when a scripts plugin hot-reloads, its old entry
    /// is overwritten (see below); a function pointer is trivially destroyed, whereas a std::function
    /// would run a move/destroy manager that lives in the just-unloaded module and crash.
    struct CppScriptRegistration
    {
        Result (*apply_overrides)(const Json&, void*) = nullptr;
        void (*bind_runtime)(void*, ScriptContext&) = nullptr;
    };

    // Registrations are keyed by the script's stable TYPE NAME (a heap string), never by std::type_index:
    // a type_index holds a type_info* into the owning module, so after a scripts plugin unloads its old
    // entries' keys would dangle and the next map rehash would read freed memory. A name key is always
    // valid, and a reload re-registers the same name, overwriting the stale entry in place.
    TBX_CPP_SCRIPTING_API void register_cpp_script_entry(
        std::string type_name,
        CppScriptRegistration entry);
    TBX_CPP_SCRIPTING_API const CppScriptRegistration* get_cpp_script_registration(
        std::string_view type_name);
    // Clears every C++ script registration. Used at shutdown before the owning modules unload.
    TBX_CPP_SCRIPTING_API void clear_cpp_script_registrations();

    /// @brief
    /// Registers a compiled C++ script type: as a normal JSON asset body (so the AssetManager can load
    /// its prototype, marked is_script for the editor), plus its override/bind callbacks in the C++
    /// script registry. Emitted by codegen for every [[tbx::script]] type.
    /// @details
    /// The apply/bind helpers are non-type template parameters so the type-erasing thunks below are
    /// non-capturing lambdas, which decay to plain function pointers stored in the registry.
    template <
        typename TScript,
        Result (*ApplyOverrides)(const Json&, TScript&),
        void (*BindRuntime)(TScript&, ScriptContext&)>
    bool register_cpp_script_type(uint32 version)
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
        register_asset_type_entry(std::move(entry));

        register_cpp_script_entry(
            std::string(tbx_serialization_type_name(static_cast<const TScript*>(nullptr))),
            CppScriptRegistration {
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
