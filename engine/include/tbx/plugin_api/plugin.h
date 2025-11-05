#pragma once
#include "tbx/application_context.h"
#include "tbx/messages/dispatcher.h"
#include "tbx/messages/message.h"
#include "tbx/plugin_api/plugin_registry.h"
#include "tbx/time/delta_time.h"
#include <functional>
#include <string>

namespace tbx
{
    using PluginDeleter = std::function<void(Plugin*)>;
    using PluginInstance = Scope<Plugin, PluginDeleter>;

    // A hot-reloadable piece of modular logic loaded at runtime.
    // Ownership: Plugins are owned by the loader/coordinator host and must
    // outlive their subscription in the message system.
    // Thread-safety: All callbacks are expected to be invoked on the thread
    // driving the application loop; plugins should internally synchronize if
    // they share state across threads.
    class TBX_API Plugin
    {
       public:
        virtual ~Plugin() = default;

        Result send_message(const Message& msg) const;

        // Called when the plugin is attached to the host.
        // The dispatcher interface allows sending or posting messages; the
        // plugin must not retain references past its own lifetime.
        virtual void on_attach(const ApplicationContext& context);

        // Called before the plugin is detached from the host
        virtual void on_detach() = 0;

        // Per-frame update with delta timing
        virtual void on_update(const DeltaTime& dt) = 0;

        // Unified message entry point for dispatch callbacks
        virtual void on_message(const Message& msg) = 0;

       protected:
        IMessageDispatcher* dispatcher() const { return _dispatcher; }

       private:
        // Does not own the dispatcher reference.
        // Comes from the application context on attach.
        IMessageDispatcher* _dispatcher = nullptr;
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
        ::tbx::PluginRegistry::instance().register_plugin(#PluginName, plugin);                    \
        return plugin;                                                                             \
    }                                                                                              \
    TBX_PLUGIN_EXPORT void destroy_##PluginName(::tbx::Plugin* plugin)                             \
    {                                                                                              \
        ::tbx::PluginRegistry::instance().unregister_plugin(#PluginName, plugin);                  \
        delete plugin;                                                                             \
    }

#define TBX_REGISTER_STATIC_PLUGIN(PluginName, PluginType)                                         \
    static PluginType PluginName##_global_instance = {};                                           \
    static bool PluginName##_global_registered = []()                                              \
    {                                                                                              \
        ::tbx::PluginRegistry::instance().register_plugin(#PluginName, &PluginName##_global_instance); \
        return true;                                                                               \
    }();                                                                                           \
    TBX_STATIC_PLUGIN_EXPORT ::tbx::Plugin* create_##PluginName()                                  \
    {                                                                                              \
        if (!PluginName##_global_registered)                                                       \
        {                                                                                          \
            ::tbx::PluginRegistry::instance().register_plugin(#PluginName, &PluginName##_global_instance); \
            PluginName##_global_registered = true;                                                 \
        }                                                                                          \
        return &PluginName##_global_instance;                                                      \
    }                                                                                              \
    TBX_STATIC_PLUGIN_EXPORT void destroy_##PluginName(::tbx::Plugin* plugin)                      \
    {                                                                                              \
        if (plugin == &PluginName##_global_instance && PluginName##_global_registered)             \
        {                                                                                          \
            ::tbx::PluginRegistry::instance().unregister_plugin(#PluginName, plugin);              \
            PluginName##_global_registered = false;                                                \
        }                                                                                          \
    }

}
