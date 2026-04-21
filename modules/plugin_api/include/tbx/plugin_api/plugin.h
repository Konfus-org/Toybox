#pragma once
#include "tbx/debugging/macros.h"
#include "tbx/messages/dispatcher.h"
#include "tbx/messages/message.h"
#include "tbx/plugin_api/plugin_registry.h"
#include "tbx/plugin_api/service_provider.h"
#include "tbx/time/delta_time.h"
#include <functional>
#include <future>
#include <string_view>
#include <type_traits>
#include <utility>

namespace tbx
{
    // Base type for runtime-loadable plugins. The runtime owns plugin lifetimes and
    // guarantees that callbacks occur on the main thread unless documented otherwise.
    class TBX_API Plugin
    {
      public:
        Plugin();
        virtual ~Plugin() noexcept;
        Plugin(const Plugin&) = delete;
        Plugin& operator=(const Plugin&) = delete;
        Plugin(Plugin&&) noexcept = default;
        Plugin& operator=(Plugin&&) = default;

        /// @brief
        /// Purpose: Initializes the plugin, wiring it to the given service provider.
        /// @details
        /// Ownership: Does not own the service provider or dispatcher references.
        /// Thread Safety: Not thread-safe; must be called exactly once before use.
        void attach(ServiceProvider& service_provider);

        /// @brief
        /// Purpose: Shuts the plugin down and clears dispatcher references.
        /// @details
        /// Ownership: Does not own the provider or dispatcher references.
        /// Thread Safety: Not thread-safe; call from the main thread.
        void detach();

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

      protected:
        // Called when the plugin is attached to the service provider.
        // The plugin must not retain references that outlive its own lifetime.
        virtual void on_attach(ServiceProvider& service_provider) = 0;

        // Called before the plugin is detached from the service provider.
        virtual void on_detach() {}

        // Per-frame update with delta timing.
        virtual void on_update(const DeltaTime& dt) {}

        // Fixed-step update with deterministic delta timing.
        virtual void on_fixed_update(const DeltaTime& dt) {}

        // Unified message entry point for dispatch callbacks.
        virtual void on_recieve_message(Message& msg) {}

        // Non-owning dispatcher reference provided by the service provider.
        IMessageDispatcher& get_dispatcher() const;

      private:
        static Result dispatcher_missing_result(std::string_view action);

        IMessageDispatcher* _dispatcher = nullptr;
    };

    using CreatePluginFn = Plugin* (*)();
    using DestroyPluginFn = void (*)(Plugin*);
}

#include "tbx/plugin_api/plugin.inl"

#if defined(TBX_PLATFORM_WINDOWS)
    #define TBX_PLUGIN_ENTRY_EXPORT extern "C" __declspec(dllexport)
#else
    #define TBX_PLUGIN_ENTRY_EXPORT extern "C"
#endif

#define TBX_REGISTER_PLUGIN(PluginName, PluginType)                                                \
    TBX_PLUGIN_ENTRY_EXPORT ::tbx::Plugin* create_##PluginName()                                   \
    {                                                                                              \
        ::tbx::Plugin* plugin = new PluginType();                                                  \
        ::tbx::PluginRegistry::get_instance().register_plugin(#PluginName, plugin);                \
        return plugin;                                                                             \
    }                                                                                              \
    TBX_PLUGIN_ENTRY_EXPORT void destroy_##PluginName(::tbx::Plugin* plugin)                       \
    {                                                                                              \
        ::tbx::PluginRegistry::get_instance().unregister_plugin(#PluginName);                      \
        delete plugin;                                                                             \
    }
