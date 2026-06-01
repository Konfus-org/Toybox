#pragma once
#include "tbx/interfaces/plugin.h"
#include "tbx/systems/plugin_api/loaded_plugin.generated.h"
#include "tbx/systems/plugin_api/plugin_meta.h"
#include "tbx/systems/plugin_api/plugin_ownership.h"
#include "tbx/systems/plugin_api/service_provider.h"
#include "tbx/systems/plugin_api/shared_library.h"

namespace tbx
{
    using PluginDeleter = std::function<void(Plugin*)>;

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
    /// Ownership: Owns `instance` and `library` (if any). Movable, non-copyable
    /// by virtue of unique_ptr semantics.
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
        LoadedPlugin(LoadedPlugin&&) noexcept = default;
        LoadedPlugin& operator=(LoadedPlugin&&) noexcept = default;

      public:
        bool is_valid() const;
        void attach(ServiceProvider& service_provider);
        void detach(ServiceProvider& service_provider);
        void receive_message(Message& msg);

        void bind_runtime(ServiceProvider& service_provider);
        void register_services(ServiceProvider& service_provider);

        void set_id(Uuid plugin_id);
        Uuid get_id() const;

      public:
        PluginMeta meta;
        std::unique_ptr<SharedLibrary> library;
        std::unique_ptr<Plugin, PluginDeleter> instance;

      private:
        RegisterPluginServicesFn _register_services = nullptr;
        BindPluginRuntimeFn _bind_runtime = nullptr;
        LoadedPluginState _state = LoadedPluginState::UNATTACHED;
        ServiceProvider* _attached_service_provider =
            nullptr; // TODO: update to be weak pointer ref, main service provider instance in app
                     // should be shared pointer
        PluginInstanceId _plugin_id = PluginInstanceId {};
        bool _services_registered = false;
    };
}
