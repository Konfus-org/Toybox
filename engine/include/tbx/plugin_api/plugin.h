#pragma once
#include "tbx/application_context.h"
#include "tbx/time/delta_time.h"
#include "tbx/messages/message.h"
#include "tbx/dispatch/dispatcher.h"

namespace tbx
{
    // Export decoration for dynamic plugin entry points defined by plugins.
    #if defined(TBX_PLATFORM_WINDOWS)
        #define TBX_PLUGIN_EXPORT extern "C" __declspec(dllexport)
    #else
        #define TBX_PLUGIN_EXPORT extern "C"
    #endif

    // Helper to register a dynamic plugin factory symbol inside a plugin module.
    // Example: TBX_REGISTER_PLUGIN(CreateMyPlugin, MyPluginType)
    #define TBX_REGISTER_PLUGIN(EntryName, PluginType) \
        TBX_PLUGIN_EXPORT ::tbx::Plugin* EntryName() { return new PluginType(); }
    
    // A hot-reloadable piece of modular logic loaded at runtime
    class Plugin
    {
    public:
        virtual ~Plugin() = default;

        // Called when the plugin is attached to the host
        virtual void on_attach(const ApplicationContext& context, MessageDispatcher& dispatcher) = 0;

        // Called before the plugin is detached from the host
        virtual void on_detach() = 0;

        // Per-frame update with delta timing
        virtual void on_update(const DeltaTime& dt) = 0;

        // Unified message entry point
        virtual void on_message(const Message& msg) = 0;
    };
}
