#include "tbx/systems/assets/serialization.h"
#include "tbx/systems/plugin_api/runtime_registrations.h"
#include "tbx/types/assets/asset.h"
#include <algorithm>
#include <memory>
#include <mutex>
#include <string>

namespace tbx
{
    // One plugin's (or the engine core's) asset-type registrations, owned by that plugin's
    // RuntimeRegistrations. Dropping the container on unload releases the loader/serializer thunks while the
    // registering module is still mapped, so no std::function manager runs after its module unmaps.
    struct AssetTypeRegistrations final : RuntimeRegistrationsData
    {
        std::mutex mutex = {};
        std::vector<AssetTypeRegistration> entries = {};
    };

    // One plugin's (or the engine core's) serializable-type registrations, owned the same way.
    struct SerializableTypeRegistrations final : RuntimeRegistrationsData
    {
        std::mutex mutex = {};
        std::vector<SerializableTypeRegistration> entries = {};
    };

    std::optional<AssetTypeRegistration> get_asset_type_registration(std::type_index type)
    {
        // Fan out engine core first; the first container that owns the type wins.
        auto found = std::optional<AssetTypeRegistration> {};
        for_each_plugin_runtime(
            [&found, type](RuntimeRegistrations& runtime)
            {
                if (found)
                    return;

                auto* data = runtime.try_get_data<AssetTypeRegistrations>();
                if (!data)
                    return;

                auto guard = std::lock_guard(data->mutex);
                const auto existing = std::ranges::find_if(
                    data->entries,
                    [type](const AssetTypeRegistration& registered)
                    {
                        return registered.type == type;
                    });
                if (existing != data->entries.end())
                    found = *existing;
            });
        return found;
    }

    std::optional<AssetTypeRegistration> get_asset_type_registration(std::string_view type_name)
    {
        if (type_name.empty())
            return std::nullopt;

        auto found = std::optional<AssetTypeRegistration> {};
        for_each_plugin_runtime(
            [&found, type_name](RuntimeRegistrations& runtime)
            {
                if (found)
                    return;

                auto* data = runtime.try_get_data<AssetTypeRegistrations>();
                if (!data)
                    return;

                auto guard = std::lock_guard(data->mutex);
                const auto existing = std::ranges::find_if(
                    data->entries,
                    [type_name](const AssetTypeRegistration& registered)
                    {
                        return registered.type_name == type_name;
                    });
                if (existing != data->entries.end())
                    found = *existing;
            });
        return found;
    }

    void register_asset_type_entry(RuntimeRegistrations& owner, AssetTypeRegistration entry)
    {
        if (entry.type == std::type_index(typeid(void)))
            return;

        // Whichever container already owns this asset type keeps it (engine core visited first), so a
        // plugin can neither shadow an engine asset type nor create a stray duplicate — it only
        // enriches an entry in its OWN container.
        auto owned_by_other = false;
        for_each_plugin_runtime(
            [&](RuntimeRegistrations& runtime)
            {
                if (&runtime == &owner)
                    return;

                auto* data = runtime.try_get_data<AssetTypeRegistrations>();
                if (!data)
                    return;

                auto guard = std::lock_guard(data->mutex);
                if (std::ranges::any_of(
                        data->entries,
                        [&entry](const AssetTypeRegistration& registered)
                        {
                            return registered.type == entry.type;
                        }))
                    owned_by_other = true;
            });

        if (owned_by_other)
            return;

        auto& data = owner.get_data<AssetTypeRegistrations>();
        auto guard = std::lock_guard(data.mutex);
        const auto existing = std::ranges::find_if(
            data.entries,
            [&entry](const AssetTypeRegistration& registered)
            {
                return registered.type == entry.type;
            });
        if (existing == data.entries.end())
        {
            data.entries.push_back(std::move(entry));
            return;
        }

        // Same container already carries this type: enrich in place.
        if (!entry.type_name.empty())
            existing->type_name = std::move(entry.type_name);
        if (entry.version != 0U)
            existing->version = entry.version;
        if (entry.create_asset)
            existing->create_asset = std::move(entry.create_asset);
        if (entry.read_body)
            existing->read_body = std::move(entry.read_body);
        if (entry.write_body)
            existing->write_body = std::move(entry.write_body);
        if (entry.transform_meta)
            existing->transform_meta = std::move(entry.transform_meta);
        if (entry.write_meta)
            existing->write_meta = std::move(entry.write_meta);
        if (entry.is_script)
            existing->is_script = true;
    }

    std::vector<AssetTypeRegistration> get_asset_type_registrations()
    {
        auto registrations = std::vector<AssetTypeRegistration> {};
        for_each_plugin_runtime(
            [&registrations](RuntimeRegistrations& runtime)
            {
                auto* data = runtime.try_get_data<AssetTypeRegistrations>();
                if (!data)
                    return;

                auto guard = std::lock_guard(data->mutex);
                registrations.insert(
                    registrations.end(),
                    data->entries.begin(),
                    data->entries.end());
            });
        return registrations;
    }

    std::vector<SerializableTypeRegistration> get_serializable_type_registrations()
    {
        auto registrations = std::vector<SerializableTypeRegistration> {};
        for_each_plugin_runtime(
            [&registrations](RuntimeRegistrations& runtime)
            {
                auto* data = runtime.try_get_data<SerializableTypeRegistrations>();
                if (!data)
                    return;

                auto guard = std::lock_guard(data->mutex);
                registrations.insert(
                    registrations.end(),
                    data->entries.begin(),
                    data->entries.end());
            });
        return registrations;
    }

    void clear_serialization_registrations()
    {
        // Shutdown: drop every container's asset-type and serializable-type thunks while their
        // modules are still mapped.
        for_each_plugin_runtime(
            [](RuntimeRegistrations& runtime)
            {
                if (auto* data = runtime.try_get_data<AssetTypeRegistrations>())
                {
                    auto guard = std::lock_guard(data->mutex);
                    data->entries.clear();
                }
                if (auto* data = runtime.try_get_data<SerializableTypeRegistrations>())
                {
                    auto guard = std::lock_guard(data->mutex);
                    data->entries.clear();
                }
            });
    }

    void register_serializable_type_entry(
        RuntimeRegistrations& owner,
        SerializableTypeRegistration entry)
    {
        if (entry.name.empty() || !entry.write_value || !entry.read_value)
            return;

        // First registration of a name wins, wherever it lives (engine core visited first). A plugin
        // re-registering an existing name is ignored, so it never comes to own that entry.
        auto already_registered = false;
        for_each_plugin_runtime(
            [&](RuntimeRegistrations& runtime)
            {
                auto* data = runtime.try_get_data<SerializableTypeRegistrations>();
                if (!data)
                    return;

                auto guard = std::lock_guard(data->mutex);
                if (std::ranges::any_of(
                        data->entries,
                        [&entry](const SerializableTypeRegistration& registered)
                        {
                            return registered.name == entry.name;
                        }))
                    already_registered = true;
            });

        if (already_registered)
            return;

        auto& data = owner.get_data<SerializableTypeRegistrations>();
        auto guard = std::lock_guard(data.mutex);
        data.entries.push_back(std::move(entry));
    }
}
