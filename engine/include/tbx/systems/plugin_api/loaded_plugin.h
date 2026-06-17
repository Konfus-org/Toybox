#pragma once
#include "tbx/interfaces/plugin.h"
#include "tbx/systems/plugin_api/loaded_plugin.generated.h"
#include "tbx/systems/plugin_api/plugin_meta.h"
#include "tbx/systems/plugin_api/plugin_ownership.h"
#include "tbx/systems/plugin_api/service_provider.h"
#include "tbx/systems/plugin_api/shared_library.h"
#include <list>

namespace tbx
{
    using PluginDeleter = std::function<void(Plugin*)>;

    // Stable node addresses keep plugin ordering snapshots valid while plugin groups are spliced.
    using LoadedPlugins = std::list<LoadedPlugin>;

    enum class LoadedPluginState
    {
        UNATTACHED,
        ATTACHED,
        DETACHED
    };

    /// @brief
    /// Represents an owned plugin instance along with its loading metadata
    /// and (optionally) the dynamic library used to load it.
    /// @details
    /// Ownership: Owns `instance` and `library` (if any). Non-copyable and non-movable; once
    /// placed in a plugin container, the loaded plugin must stay in place until destruction.
    /// Thread-safety: Not thread-safe; expected to be used by the main thread.
    [[printable("Name={}, Version={}", meta.name, meta.version)]];
    class TBX_API LoadedPlugin
    {
      public:
        LoadedPlugin(
            PluginMeta meta_data,
            std::unique_ptr<SharedLibrary> plugin_library,
            std::unique_ptr<Plugin, PluginDeleter> plugin_instance,
            RegisterPluginServicesFn register_services = nullptr,
            BindPluginRuntimeFn bind_runtime = nullptr);
        ~LoadedPlugin() noexcept;

      public:
        LoadedPlugin() = default;
        LoadedPlugin(const LoadedPlugin&) = delete;
        LoadedPlugin& operator=(const LoadedPlugin&) = delete;
        LoadedPlugin(LoadedPlugin&&) noexcept = delete;
        LoadedPlugin& operator=(LoadedPlugin&&) noexcept = delete;

      public:
        bool is_valid() const;
        bool is_attached() const;
        void attach(std::shared_ptr<ServiceProvider> service_provider);
        void detach(ServiceProvider& service_provider);
        void fixed_update(const DeltaTime& dt);
        void receive_message(Message& msg);
        void update(const DeltaTime& dt);

        void bind_runtime(ServiceProvider& service_provider);
        void register_services(ServiceProvider& service_provider);

        void set_id(Uuid plugin_id);
        Uuid get_id() const;

      public:
        PluginMeta meta;
        std::unique_ptr<SharedLibrary> library;
        std::unique_ptr<Plugin, PluginDeleter> instance;

      private:
        PluginInstanceId _plugin_id = PluginInstanceId {};
        std::weak_ptr<ServiceProvider> _attached_service_provider = {};
        RegisterPluginServicesFn _register_services = nullptr;
        BindPluginRuntimeFn _bind_runtime = nullptr;
        LoadedPluginState _state = LoadedPluginState::UNATTACHED;
        bool _services_registered = false;
    };
}
