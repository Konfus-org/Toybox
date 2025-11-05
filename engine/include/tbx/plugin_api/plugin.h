#pragma once
#include "tbx/application_context.h"
#include "tbx/messages/dispatcher.h"
#include "tbx/messages/message.h"
#include "tbx/plugin_api/plugin_registry.h"
#include "tbx/time/delta_time.h"
#include <string>

namespace tbx
{
#if defined(TBX_PLATFORM_WINDOWS)
    #define TBX_PLUGIN_EXPORT extern "C" __declspec(dllexport)
    #define TBX_STATIC_PLUGIN_EXPORT extern "C"
#else
    #define TBX_PLUGIN_EXPORT extern "C"
    #define TBX_STATIC_PLUGIN_EXPORT extern "C"
#endif

#define TBX_DETAIL_DEFINE_PLUGIN_ENTRY(ExportMacro, EntryName, PluginType)                         \
    ExportMacro ::tbx::Plugin* EntryName()                                                         \
    {                                                                                              \
        ::tbx::Plugin* plugin = new PluginType();                                                  \
        ::tbx::PluginRegistry::instance().register_plugin(#EntryName, plugin);                     \
        return plugin;                                                                             \
    }                                                                                              \
    ExportMacro void EntryName##_Destroy(::tbx::Plugin* plugin)                                    \
    {                                                                                              \
        ::tbx::PluginRegistry::instance().unregister_plugin(plugin);                               \
        delete plugin;                                                                             \
    }

    class Application;

    class Plugin;

    using CreatePluginFn = Plugin*(*)();
    using DestroyPluginFn = void (*)(Plugin*);

    class TBX_API StaticPluginRegistration
    {
       public:
        StaticPluginRegistration(const char* entry_point, CreatePluginFn create, DestroyPluginFn destroy);
        ~StaticPluginRegistration();

       private:
        std::string _entry_point;
    };

#define TBX_REGISTER_PLUGIN(EntryName, PluginType)                                                 \
    TBX_DETAIL_DEFINE_PLUGIN_ENTRY(TBX_PLUGIN_EXPORT, EntryName, PluginType)

#define TBX_REGISTER_STATIC_PLUGIN(EntryName, PluginType)                                          \
    TBX_DETAIL_DEFINE_PLUGIN_ENTRY(TBX_STATIC_PLUGIN_EXPORT, EntryName, PluginType);               \
    static ::tbx::StaticPluginRegistration EntryName##_static_registration(                        \
        #EntryName,                                                                                \
        &EntryName,                                                                                \
        &EntryName##_Destroy)

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
        virtual void on_attach(const ApplicationContext& context) = 0;

        // Called before the plugin is detached from the host
        virtual void on_detach() = 0;

        // Per-frame update with delta timing
        virtual void on_update(const DeltaTime& dt) = 0;

        // Unified message entry point for dispatch callbacks
        virtual void on_message(const Message& msg) = 0;

       protected:
        IMessageDispatcher* dispatcher() const noexcept;

       private:
        friend class Application;
        void set_host(Application* application) noexcept;

        IMessageDispatcher* _dispatcher = nullptr;
    };
}
