#pragma once
#include "tbx/interfaces/message_dispatcher.h"
#include "tbx/systems/messaging/message.h"
#include "tbx/systems/plugin_api/plugin_meta.h"
#include "tbx/systems/plugin_api/plugin_ownership.h"
#include "tbx/systems/plugin_api/plugin_registry.h"
#include "tbx/systems/plugin_api/service_provider.h"
#include "tbx/systems/time/delta_time.h"
#include <future>

namespace tbx
{
    using GetPluginMetaFn = void (*)(PluginMeta*);
    using CreatePluginFn = Plugin* (*)();
    using DestroyPluginFn = void (*)(Plugin*);
    using BindPluginRuntimeFn = void (*)(Plugin*, ServiceProvider*);
    using RegisterPluginServicesFn = void (*)(Plugin*, ServiceProvider*);
    using RegisterPluginScriptsFn = void (*)();
    using UnregisterPluginScriptsFn = void (*)();

    // Base type for runtime-loadable plugins. The runtime owns plugin lifetimes and
    // guarantees that callbacks occur on the main thread unless documented otherwise.
    class TBX_API Plugin
    {
      public:
        Plugin();
        virtual ~Plugin() noexcept;

      public:
        Plugin(const Plugin&) = delete;
        Plugin& operator=(const Plugin&) = delete;
        Plugin(Plugin&&) noexcept = default;
        Plugin& operator=(Plugin&&) = default;

      public:
        // Initializes the plugin, wiring it to the given service provider.
        void attach(ServiceProvider& service_provider, PluginInstanceId plugin_id);

        // Shuts the plugin down and clears dispatcher references.
        void detach(ServiceProvider& service_provider);

        // Ticks the plugin for the given frame delta.
        void update(const DeltaTime& dt);

        // Ticks the plugin for a fixed simulation step.
        void fixed_update(const DeltaTime& dt);

        // Invokes the plugin's message entry point.
        void receive_message(Message& msg);

        // Helper to synchronously send a constructed message via the provider dispatcher.
        template <typename TMessage, typename... TArgs>
            requires std::derived_from<TMessage, Message>
        Result send_message(TArgs&&... args) const;

        // Helper to post a constructed message for deferred processing via the dispatcher.
        template <typename TMessage, typename... TArgs>
            requires std::derived_from<TMessage, Message>
        std::shared_future<Result> post_message(TArgs&&... args) const;

        Uuid get_id() const;

      protected:
        // Called when the plugin is attached to the service provider.
        // The plugin must not retain references that outlive its own lifetime.
        virtual void on_attach() {}

        // Called before the plugin is detached from the service provider.
        virtual void on_detach() {}

        // Per-frame update with delta timing.
        virtual void on_update(const DeltaTime& dt) {}

        // Fixed-step update with deterministic delta timing.
        virtual void on_fixed_update(const DeltaTime& dt) {}

        // Unified message entry point for dispatch callbacks.
        virtual void on_recieve_message(Message& msg) {}

        // Non-owning dispatcher service provided by the service provider.
        IMessageDispatcher& get_dispatcher() const;

      private:
        static Result dispatcher_missing_result(std::string_view action);

        std::weak_ptr<IMessageDispatcher> _dispatcher = {};
        PluginInstanceId _plugin_id = PluginInstanceId {};
    };
}

#include "tbx/interfaces/plugin.inl"

#if defined(TBX_PLATFORM_WINDOWS)
    #define TBX_PLUGIN_ENTRY_EXPORT extern "C" __declspec(dllexport)
#else
    #define TBX_PLUGIN_ENTRY_EXPORT extern "C"
#endif
