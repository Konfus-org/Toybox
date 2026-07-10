#include "tbx/systems/scripting/script.h"
#include "tbx/systems/debugging/macros.h"
#include "tbx/systems/ecs/entity.h"
#include "tbx/systems/scripting/script_context.h"
#include "tbx/types/assets/world.h"
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>

namespace tbx
{
    struct ScriptStorageRecord
    {
        std::optional<ScriptContext> context = std::nullopt;
        std::unordered_map<std::string, ScriptBinding> script_references = {};
    };

    static std::unordered_map<const Script*, std::unique_ptr<ScriptStorageRecord>>& script_storage()
    {
        static auto g_script_storage =
            std::unordered_map<const Script*, std::unique_ptr<ScriptStorageRecord>> {};
        return g_script_storage;
    }

    static std::mutex& script_storage_mutex()
    {
        static auto g_script_storage_mutex = std::mutex {};
        return g_script_storage_mutex;
    }

    static ScriptStorageRecord* try_find_script_storage(const Script& script)
    {
        auto guard = std::lock_guard(script_storage_mutex());
        const auto iterator = script_storage().find(&script);
        if (iterator == script_storage().end())
            return nullptr;

        return iterator->second.get();
    }

    static ScriptStorageRecord& get_or_create_script_storage(const Script& script)
    {
        auto guard = std::lock_guard(script_storage_mutex());
        auto& storage = script_storage()[&script];
        if (!storage)
            storage = std::make_unique<ScriptStorageRecord>();

        return *storage;
    }

    static void erase_script_storage(const Script& script)
    {
        auto guard = std::lock_guard(script_storage_mutex());
        script_storage().erase(&script);
    }

    Script::~Script() noexcept
    {
        erase_script_storage(*this);
    }

    void Script::bind(ScriptContext context)
    {
        get_or_create_script_storage(*this).context = std::move(context);
    }

    std::optional<ScriptBinding> Script::get_script_binding() const
    {
        auto* storage = try_find_script_storage(*this);
        if (storage == nullptr || !storage->context.has_value())
            return std::nullopt;

        return storage->context->get_script_binding();
    }

    std::optional<ScriptBinding> Script::get_script_reference(std::string_view field_name) const
    {
        auto* storage = try_find_script_storage(*this);
        if (storage == nullptr)
            return std::nullopt;

        const auto iterator = storage->script_references.find(std::string(field_name));
        if (iterator == storage->script_references.end())
            return std::nullopt;

        return iterator->second;
    }

    Entity& Script::get_entity() const
    {
        auto* storage = try_find_script_storage(*this);
        TBX_ASSERT(storage != nullptr && storage->context.has_value(), "Script has no bound context.");
        return storage->context->get_entity();
    }

    ServiceProvider& Script::get_services() const
    {
        auto* storage = try_find_script_storage(*this);
        TBX_ASSERT(storage != nullptr && storage->context.has_value(), "Script has no bound context.");
        return storage->context->get_services();
    }

    std::weak_ptr<World> Script::get_world_ptr() const
    {
        auto* storage = try_find_script_storage(*this);
        TBX_ASSERT(storage != nullptr && storage->context.has_value(), "Script has no bound context.");
        return storage->context->get_world_ptr();
    }

    World& Script::get_world() const
    {
        auto* storage = try_find_script_storage(*this);
        TBX_ASSERT(storage != nullptr && storage->context.has_value(), "Script has no bound context.");
        return storage->context->get_world();
    }

    void Script::set_script_reference(std::string_view field_name, ScriptBinding binding)
    {
        get_or_create_script_storage(*this).script_references[std::string(field_name)] = binding;
    }
}
