#include "cpp_scripting_backend.h"
#include "tbx/systems/assets/manager.h"
#include "tbx/systems/debugging/macros.h"
#include "tbx/systems/scripting/script.h"
#include "tbx/systems/scripting/script_registry.h"
#include <string>
#include <typeindex>
#include <utility>

namespace cpp_scripting
{
    // Deep-copies a script prototype by routing it through its registered body serializer, so a fresh
    // instance starts from the asset's authored default values before per-binding overrides apply.
    static std::shared_ptr<tbx::Script> clone_script_prototype(const tbx::Script& prototype)
    {
        auto registration = tbx::get_asset_type_registration(std::type_index(typeid(prototype)));
        if (!registration.has_value() || !registration->create_asset)
            return {};

        auto asset = registration->create_asset();
        if (!asset)
            return {};

        if (registration->write_body && registration->read_body)
        {
            auto data = std::string();
            if (!registration->write_body(&prototype, data).succeeded())
                return {};
            if (!registration->read_body(data, asset.get()).succeeded())
                return {};
        }

        asset->id = prototype.id;
        asset->version = prototype.version;
        auto* script = dynamic_cast<tbx::Script*>(asset.release());
        if (script == nullptr)
            return {};

        return std::shared_ptr<tbx::Script>(script);
    }

    CppScriptingBackend::CppScriptingBackend(std::weak_ptr<tbx::AssetManager> asset_manager)
        : _asset_manager(std::move(asset_manager))
    {
    }

    tbx::ScriptLanguageInfo CppScriptingBackend::language() const
    {
        // A C++ script asset is identified by its declaration header; the .cpp is a build input.
        return tbx::ScriptLanguageInfo { .name = "cpp", .extensions = { ".h" } };
    }

    std::shared_ptr<tbx::IScriptInstance> CppScriptingBackend::instantiate(
        const tbx::Handle& script,
        const tbx::Json& overrides)
    {
        auto asset_manager = _asset_manager.lock();
        if (!asset_manager)
            return {};

        auto prototype_asset = asset_manager->load(script);
        auto prototype = std::dynamic_pointer_cast<tbx::Script>(prototype_asset);
        if (!prototype)
            return {}; // Not a C++ script asset; another backend may own it.

        auto instance = clone_script_prototype(*prototype);
        if (!instance)
        {
            TBX_TRACE_WARNING("Failed to create script instance id={}.", script.id);
            return {};
        }

        auto* instance_ptr = instance.get();
        const auto asset_registration =
            tbx::get_asset_type_registration(std::type_index(typeid(*instance_ptr)));
        const auto* script_registration =
            asset_registration.has_value()
                ? tbx::get_script_registration(asset_registration->type_name)
                : nullptr;
        if (script_registration != nullptr && script_registration->apply_overrides
            && !overrides.is_null() && !overrides.empty())
        {
            auto result = script_registration->apply_overrides(overrides, instance.get());
            if (!result.succeeded())
            {
                TBX_TRACE_WARNING(
                    "Failed to apply script overrides id={}: {}",
                    script.id,
                    result.get_report());
                return {};
            }
        }

        return instance;
    }

    void CppScriptingBackend::bind(tbx::IScriptInstance& instance, tbx::ScriptContext& context)
    {
        auto* script = dynamic_cast<tbx::Script*>(&instance);
        if (script == nullptr)
            return;

        script->bind(context);

        const auto asset_registration =
            tbx::get_asset_type_registration(std::type_index(typeid(*script)));
        if (asset_registration.has_value())
        {
            if (const auto* script_registration =
                    tbx::get_script_registration(asset_registration->type_name);
                script_registration != nullptr && script_registration->bind_runtime)
                script_registration->bind_runtime(script, context);
        }
    }
}
