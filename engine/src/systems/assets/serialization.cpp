#include "tbx/systems/assets/serialization.h"
#include "tbx/systems/assets/describe.h"
#include "tbx/systems/plugin_api/plugin_ownership.h"
#include "tbx/systems/plugin_api/plugin_ownership_tracking.h"
#include "tbx/types/assets/asset.h"
#include <memory>
#include <mutex>

namespace tbx
{
    class SerializationRegistrationStore final
    {
      public:
        static SerializationRegistrationStore& get_instance()
        {
            static SerializationRegistrationStore store = {};
            return store;
        }

      public:
        SerializationRegistrationStore(const SerializationRegistrationStore&) = delete;
        SerializationRegistrationStore& operator=(const SerializationRegistrationStore&) = delete;
        SerializationRegistrationStore(SerializationRegistrationStore&&) = delete;
        SerializationRegistrationStore& operator=(SerializationRegistrationStore&&) = delete;

      public:
        std::mutex& asset_type_mutex()
        {
            return _asset_type_mutex;
        }

        std::vector<AssetTypeRegistration>& asset_types()
        {
            return _asset_types;
        }

        std::mutex& serializable_type_mutex()
        {
            return _serializable_type_mutex;
        }

        std::vector<SerializableTypeRegistration>& serializable_types()
        {
            return _serializable_types;
        }

      private:
        SerializationRegistrationStore() = default;
        ~SerializationRegistrationStore() noexcept = default;

      private:
        std::mutex _asset_type_mutex = {};
        std::vector<AssetTypeRegistration> _asset_types = {};
        std::mutex _serializable_type_mutex = {};
        std::vector<SerializableTypeRegistration> _serializable_types = {};
    };

    std::optional<AssetTypeRegistration> get_asset_type_registration(std::type_index type)
    {
        auto& store = SerializationRegistrationStore::get_instance();
        auto guard = std::lock_guard(store.asset_type_mutex());
        const auto& registrations = store.asset_types();
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

        auto& store = SerializationRegistrationStore::get_instance();
        auto guard = std::lock_guard(store.asset_type_mutex());
        const auto& registrations = store.asset_types();
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

        auto& store = SerializationRegistrationStore::get_instance();
        auto guard = std::lock_guard(store.asset_type_mutex());
        auto& registrations = store.asset_types();
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
        if (entry.type == std::type_index(typeid(void)))
            return;

        const auto registration_type = entry.type;

        auto& store = SerializationRegistrationStore::get_instance();
        auto guard = std::lock_guard(store.asset_type_mutex());
        auto& registrations = store.asset_types();
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
            if (entry.describe)
                existing->describe = std::move(entry.describe);
            if (entry.is_script)
                existing->is_script = true;
            return;
        }

        track_plugin_owned_asset_type(registration_type);
    }

    std::vector<AssetTypeRegistration> get_asset_type_registrations()
    {
        auto& store = SerializationRegistrationStore::get_instance();
        auto guard = std::lock_guard(store.asset_type_mutex());
        return store.asset_types();
    }

    std::vector<SerializableTypeRegistration> get_serializable_type_registrations()
    {
        auto& store = SerializationRegistrationStore::get_instance();
        auto guard = std::lock_guard(store.serializable_type_mutex());
        return store.serializable_types();
    }

    void unregister_serializable_type_entry(std::string_view name)
    {
        if (name.empty())
            return;

        auto& store = SerializationRegistrationStore::get_instance();
        auto guard = std::lock_guard(store.serializable_type_mutex());
        auto& registrations = store.serializable_types();
        const auto iterator = std::ranges::find_if(
            registrations,
            [name](const SerializableTypeRegistration& registration)
            {
                return registration.name == name;
            });
        if (iterator != registrations.end())
            registrations.erase(iterator);
    }

    void clear_serialization_registrations()
    {
        auto& store = SerializationRegistrationStore::get_instance();
        {
            auto guard = std::lock_guard(store.serializable_type_mutex());
            store.serializable_types().clear();
        }
        {
            auto guard = std::lock_guard(store.asset_type_mutex());
            store.asset_types().clear();
        }
    }

    std::string describe_serializable_asset(std::string_view type_name)
    {
        const auto registration = get_asset_type_registration(type_name);
        if (!registration || !registration->create_asset || !registration->write_body)
            return {};

        auto asset = registration->create_asset();
        if (!asset)
            return {};

        // Enter the editor scopes here, in the engine module, so the generated serialize that write_body
        // invokes — which reads a per-module thread-local switch — actually emits the enriched, every-field
        // shape rather than the lean persistence form.
        const auto include_all = OmitDefaultFieldsScope(false);
        const auto include_attrs = AttributeSerializationScope(true);
        auto body = std::string();
        if (!registration->write_body(asset.get(), body))
            return {};

        return body;
    }

    void register_serializable_type_entry(SerializableTypeRegistration entry)
    {
        if (entry.name.empty() || !entry.write_value || !entry.read_value)
            return;

        const auto registration_name = entry.name;

        auto& store = SerializationRegistrationStore::get_instance();
        auto guard = std::lock_guard(store.serializable_type_mutex());
        auto& registrations = store.serializable_types();
        const auto existing = std::ranges::find_if(
            registrations,
            [&entry](const SerializableTypeRegistration& registered)
            {
                return registered.name == entry.name;
            });
        if (existing != registrations.end())
            return;

        registrations.push_back(std::move(entry));

        track_plugin_owned_serializable_registration(registration_name);
    }
}
