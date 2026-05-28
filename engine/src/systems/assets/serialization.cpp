#include "tbx/systems/assets/serialization.h"
#include "systems/assets/internal/serialization_internal.h"
#include <algorithm>

namespace tbx
{
    std::optional<AssetTypeRegistration> get_asset_type_registration(std::type_index type)
    {
        auto guard = std::lock_guard(internal::asset_type_registration_mutex());
        const auto& registrations = internal::asset_type_registrations();
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

    void register_asset_type_entry(AssetTypeRegistration entry)
    {
        if (entry.type == std::type_index(typeid(void)))
            return;

        auto guard = std::lock_guard(internal::asset_type_registration_mutex());
        auto& registrations = internal::asset_type_registrations();
        const auto existing = std::ranges::find_if(
            registrations,
            [&entry](const AssetTypeRegistration& registered)
            {
                return registered.type == entry.type;
            });

        if (existing == registrations.end())
        {
            registrations.push_back(std::move(entry));
            return;
        }

        if (!entry.type_name.empty())
            existing->type_name = std::move(entry.type_name);
        if (entry.version != 0U)
            existing->version = entry.version;
        if (entry.read_body)
            existing->read_body = std::move(entry.read_body);
        if (entry.write_body)
            existing->write_body = std::move(entry.write_body);
        if (entry.transform_meta)
            existing->transform_meta = std::move(entry.transform_meta);
    }

    std::vector<SerializableTypeRegistration> get_serializable_type_registrations()
    {
        auto guard = std::lock_guard(internal::serializable_type_registration_mutex());
        return internal::serializable_type_registrations();
    }

    void register_serializable_type_entry(SerializableTypeRegistration entry)
    {
        if (entry.name.empty() || !entry.write_value || !entry.read_value)
            return;

        auto guard = std::lock_guard(internal::serializable_type_registration_mutex());
        auto& registrations = internal::serializable_type_registrations();
        const auto existing = std::ranges::find_if(
            registrations,
            [&entry](const SerializableTypeRegistration& registered)
            {
                return registered.name == entry.name;
            });
        if (existing != registrations.end())
        {
            *existing = std::move(entry);
            return;
        }

        registrations.push_back(std::move(entry));
    }
}
