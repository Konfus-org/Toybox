#include "tbx/systems/assets/serialization.h"
#include "tbx/systems/plugin_api/plugin_loader.h"
#include "tbx/systems/plugin_api/plugin_ownership.h"
#include "tbx/systems/plugin_api/plugin_ownership_tracker.h"

namespace tbx
{
    static std::vector<AssetTypeRegistration>& asset_type_registrations()
    {
        static auto registrations = std::vector<AssetTypeRegistration> {};
        return registrations;
    }

    static std::mutex& asset_type_registration_mutex()
    {
        static auto mutex = std::mutex();
        return mutex;
    }

    static std::vector<SerializableTypeRegistration>& serializable_type_registrations()
    {
        static auto registrations = std::vector<SerializableTypeRegistration> {};
        return registrations;
    }

    static std::mutex& serializable_type_registration_mutex()
    {
        static auto mutex = std::mutex();
        return mutex;
    }

    std::optional<AssetTypeRegistration> get_asset_type_registration(std::type_index type)
    {
        auto guard = std::lock_guard(asset_type_registration_mutex());
        const auto& registrations = asset_type_registrations();
        const auto existing = std::ranges::find_if(
            registrations,
            [type](const AssetTypeRegistration& registered)
            {
                return registered.type == type;
            });
        if (existing == registrations.end())
            return std::nullopt;

        return *existing;
    }

    std::optional<AssetTypeRegistration> get_asset_type_registration(std::string_view type_name)
    {
        if (type_name.empty())
            return std::nullopt;

        auto guard = std::lock_guard(asset_type_registration_mutex());
        const auto& registrations = asset_type_registrations();
        const auto existing = std::ranges::find_if(
            registrations,
            [type_name](const AssetTypeRegistration& registered)
            {
                return registered.type_name == type_name;
            });
        if (existing == registrations.end())
            return std::nullopt;

        return *existing;
    }

    void unregister_asset_type_entry(std::type_index asset_type)
    {
        if (asset_type == std::type_index(typeid(void)))
            return;

        auto guard = std::lock_guard(asset_type_registration_mutex());
        auto& registrations = asset_type_registrations();
        const auto iterator = std::ranges::find_if(
            registrations,
            [asset_type](const AssetTypeRegistration& registration)
            {
                return registration.type == asset_type;
            });
        if (iterator != registrations.end())
            registrations.erase(iterator);
    }

    void register_asset_type_entry(AssetTypeRegistration entry)
    {
        if (is_plugin_meta_query_active())
            return;

        if (entry.type == std::type_index(typeid(void)))
            return;

        const auto registration_type = entry.type;

        auto guard = std::lock_guard(asset_type_registration_mutex());
        auto& registrations = asset_type_registrations();
        const auto existing = std::ranges::find_if(
            registrations,
            [&entry](const AssetTypeRegistration& registered)
            {
                return registered.type == entry.type;
            });
        if (existing == registrations.end())
        {
            registrations.push_back(std::move(entry));
        }
        else
        {
            // Prevent plugin code from replacing engine-owned callbacks for existing asset types.
            if (has_active_plugin_id())
                return;

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
            if (entry.apply_overrides)
                existing->apply_overrides = std::move(entry.apply_overrides);
            if (entry.bind_runtime)
                existing->bind_runtime = std::move(entry.bind_runtime);
            return;
        }

        if (!has_active_plugin_id())
            return;

        if (auto tracker = lock_plugin_ownership_tracker())
            tracker->track_asset_type_registration(get_active_plugin_id(), registration_type);
    }

    std::vector<SerializableTypeRegistration> get_serializable_type_registrations()
    {
        auto guard = std::lock_guard(serializable_type_registration_mutex());
        return serializable_type_registrations();
    }

    void unregister_serializable_type_entry(std::string_view name)
    {
        if (name.empty())
            return;

        auto guard = std::lock_guard(serializable_type_registration_mutex());
        auto& registrations = serializable_type_registrations();
        const auto iterator = std::ranges::find_if(
            registrations,
            [name](const SerializableTypeRegistration& registration)
            {
                return registration.name == name;
            });
        if (iterator != registrations.end())
            registrations.erase(iterator);
    }

    void register_serializable_type_entry(SerializableTypeRegistration entry)
    {
        if (is_plugin_meta_query_active())
            return;

        if (entry.name.empty() || !entry.write_value || !entry.read_value)
            return;

        const auto registration_name = entry.name;

        auto guard = std::lock_guard(serializable_type_registration_mutex());
        auto& registrations = serializable_type_registrations();
        const auto existing = std::ranges::find_if(
            registrations,
            [&entry](const SerializableTypeRegistration& registered)
            {
                return registered.name == entry.name;
            });
        if (existing != registrations.end())
            return;

        registrations.push_back(std::move(entry));

        if (!has_active_plugin_id())
            return;

        if (auto tracker = lock_plugin_ownership_tracker())
            tracker->track_serializable_registration(get_active_plugin_id(), registration_name);
    }
}
