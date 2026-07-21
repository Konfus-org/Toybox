#pragma once
#include "tbx/systems/plugin_api/plugin_ownership.h"
#include "tbx/tbx_api.h"
#include "tbx/types/handle.h"
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <type_traits>
#include <typeindex>
#include <unordered_map>
#include <vector>

namespace tbx
{
    /// @brief
    /// Purpose: Base for one domain's per-module registration store — components, scripts, asset
    /// types, serializable types, services, asset dirs/pins. A domain defines a derived type IN ITS
    /// OWN translation unit (so plugin_api pulls in none of those headers) holding that domain's
    /// registrations; the derived destructor releases them — and, for components, strips their
    /// instances from every live registry. RuntimeRegistrations owns one instance of each domain
    /// type a module actually touches, keyed by the domain type.
    class TBX_API RuntimeRegistrationsData
    {
      public:
        RuntimeRegistrationsData() = default;
        virtual ~RuntimeRegistrationsData() noexcept = default;

      public:
        RuntimeRegistrationsData(const RuntimeRegistrationsData&) = delete;
        RuntimeRegistrationsData& operator=(const RuntimeRegistrationsData&) = delete;
        RuntimeRegistrationsData(RuntimeRegistrationsData&&) = delete;
        RuntimeRegistrationsData& operator=(RuntimeRegistrationsData&&) = delete;

      public:
        /// @brief Called when a PLUGIN's container is being dropped on unload, before the store is
        /// destroyed — the component store uses it to strip its instances from every live registry.
        /// NOT called for the engine-core container (which is only destroyed at process exit, when
        /// touching other engine statics would be unsafe and pointless — engine types live forever).
        virtual void on_container_unloading() {}
    };

    /// @brief
    /// Purpose: Owns every registration a single module (a plugin, the app module, or the engine
    /// core) contributes — its component / script / asset-type / serializable-type registrations,
    /// plus the services and asset directories/pins it added. Unloading a plugin becomes a single
    /// container drop instead of a tracked reverse-unregister: destroying the RuntimeRegistrations
    /// releases everything the module registered.
    /// @details
    /// Ownership: "Per module" is about lifetime and grouping — the container itself is engine-owned
    /// (hung off the host-side LoadedPlugin), and the engine core has one permanent instance for its
    /// built-ins. Entities are NOT owned here: a module only owns TYPES, never instances.
    /// Thread Safety: Registration and lookup run on the main thread. The process-wide list of live
    /// containers is mutex-guarded because plugin loads/unloads and registration lookups can
    /// interleave (e.g. asset streaming on a worker resolving a serializable type while a plugin
    /// loads).
    class TBX_API RuntimeRegistrations final
    {
      public:
        explicit RuntimeRegistrations(PluginInstanceId id);
        ~RuntimeRegistrations() noexcept;

      public:
        RuntimeRegistrations(const RuntimeRegistrations&) = delete;
        RuntimeRegistrations& operator=(const RuntimeRegistrations&) = delete;
        RuntimeRegistrations(RuntimeRegistrations&&) = delete;
        RuntimeRegistrations& operator=(RuntimeRegistrations&&) = delete;

      public:
        PluginInstanceId get_id() const;

        /// @brief Returns this container's storage for domain T, creating it on first access. T must
        /// derive RuntimeRegistrationsData. The returned reference stays valid for the container's
        /// lifetime (entries are only dropped when the whole container is destroyed). Thread-safe.
        template <typename T>
        T& get_data();

        /// @brief Returns this container's storage for domain T, or nullptr when it has none.
        /// Thread-safe. Used by fan-out lookups that must skip containers a domain never touched.
        template <typename T>
        T* try_get_data();

        /// @brief Records a module-owned service type / asset directory / pinned handle so the
        /// unloader can release it. Each no-ops for the engine core: its services and directories
        /// live for the whole process and are torn down by the ServiceProvider / AssetManager
        /// themselves, never by a container.
        void add_service(std::type_index service_type);
        void add_asset_directory(const std::filesystem::path& directory);
        void add_pin(const Handle& handle);

        /// @brief The recorded resources, read by the unloader while the plugin's library is still
        /// mapped. Only valid to read once registration has quiesced (i.e. during unload).
        const std::vector<std::type_index>& get_services() const;
        const std::vector<std::filesystem::path>& get_asset_directories() const;
        const std::vector<Handle>& get_pinned_handles() const;

      private:
        PluginInstanceId _id = {};
        // Guards the domain map and the resource lists below. Each domain store still owns its own
        // synchronization for its entries.
        std::mutex _data_mutex = {};
        std::unordered_map<std::type_index, std::unique_ptr<RuntimeRegistrationsData>> _data = {};
        std::vector<std::type_index> _services = {};
        std::vector<std::filesystem::path> _asset_directories = {};
        std::vector<Handle> _pinned_handles = {};
    };

    /// @brief
    /// Purpose: The engine-core container. Registrations made with no active plugin (engine
    /// built-ins) land here. Never destroyed (process lifetime) and always visited FIRST by the
    /// fan-out below, so a plugin can never shadow an engine-owned entry.
    TBX_API RuntimeRegistrations& engine_core_runtime();

    /// @brief
    /// Purpose: The container registrations are currently routed to — the plugin whose
    /// ScopedPluginContext is active on this thread, or the engine-core container when none is.
    TBX_API RuntimeRegistrations& active_plugin_runtime();

    /// @brief
    /// Purpose: Invokes the callback for every live container, engine core first then plugins in
    /// load order. Registration lookups fan out through this so engine-owned entries win a collision.
    /// @details
    /// Thread Safety: The plugin list is snapshotted under the registry lock before the callback
    /// runs, so a load/unload triggered from within the callback cannot invalidate iteration.
    TBX_API void for_each_plugin_runtime(const std::function<void(RuntimeRegistrations&)>& callback);
}

#include "tbx/systems/plugin_api/runtime_registrations.inl"
