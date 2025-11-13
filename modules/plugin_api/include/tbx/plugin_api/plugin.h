#pragma once
#include "tbx/messages/dispatcher.h"
#include "tbx/messages/message.h"
#include "tbx/plugin_api/plugin_registry.h"
#include "tbx/time/delta_time.h"
#include <functional>

namespace tbx
{
    class Application;

    // Base type for runtime-loadable plugins. The host owns plugin lifetimes and
    // guarantees that callbacks occur on the main thread unless documented otherwise.
    class TBX_API Plugin
    {
      public:
        Plugin();
        virtual ~Plugin();
        Plugin(const Plugin&) = delete;
        Plugin& operator=(const Plugin&) = delete;
        Plugin(Plugin&&) = default;
        Plugin& operator=(Plugin&&) = default;

        // Initializes the plugin, wiring it to the given host and dispatcher.
        // Must be called exactly once before any other interaction.
        void attach(Application& host);

        // Shuts the plugin down and clears host/dispatcher references.
        void detach();

        // Ticks the plugin for the given frame delta.
        void update(const DeltaTime& dt);

        // Invokes the plugin's message entry point.
        void receive_message(Message& msg);

        // Helper to synchronously send a message via the host dispatcher.
        Result send_message(Message& msg) const;

        // Helper to post a message for deferred processing via the dispatcher.
        Result post_message(Message& msg) const;

      protected:
        // Called when the plugin is attached to the host.
        // The plugin must not retain references that outlive its own lifetime.
        virtual void on_attach(Application& host) = 0;

        // Called before the plugin is detached from the host.
        virtual void on_detach() = 0;

        // Per-frame update with delta timing.
        virtual void on_update(const DeltaTime& dt) = 0;

        // Unified message entry point for dispatch callbacks.
        virtual void on_message(Message& msg) = 0;

        // Non-owning dispatcher reference provided by the host.
        IMessageDispatcher& get_dispatcher() const;

        // Non-owning reference to the host Application.
        Application& get_host() const;

      private:
        IMessageDispatcher* _dispatcher = nullptr;
        Application* _host = nullptr;
    };

    using CreatePluginFn = Plugin* (*)();
    using DestroyPluginFn = void (*)(Plugin*);

#if defined(TBX_PLATFORM_WINDOWS)
    #define TBX_PLUGIN_EXPORT extern "C" __declspec(dllexport)
    #define TBX_STATIC_PLUGIN_EXPORT extern "C"
#else
    #define TBX_PLUGIN_EXPORT extern "C"
    #define TBX_STATIC_PLUGIN_EXPORT extern "C"
#endif

#define TBX_REGISTER_PLUGIN(PluginName, PluginType)                                                \
    TBX_PLUGIN_EXPORT ::tbx::Plugin* create_##PluginName()                                         \
    {                                                                                              \
        ::tbx::Plugin* plugin = new PluginType();                                                  \
        ::tbx::PluginRegistry::get_instance().register_plugin(#PluginName, plugin);                \
        return plugin;                                                                             \
    }                                                                                              \
    TBX_PLUGIN_EXPORT void destroy_##PluginName(::tbx::Plugin* plugin)                             \
    {                                                                                              \
        ::tbx::PluginRegistry::get_instance().unregister_plugin(#PluginName);                      \
        delete plugin;                                                                             \
    }

#define TBX_REGISTER_STATIC_PLUGIN(PluginName, PluginType)                                         \
    static PluginType PluginName##_global_instance = {};                                           \
    static bool PluginName##_registered = []()                                                     \
    {                                                                                              \
        ::tbx::PluginRegistry::get_instance().register_plugin(                                     \
            #PluginName,                                                                           \
            &PluginName##_global_instance);                                                        \
        return true;                                                                               \
    }();

}
