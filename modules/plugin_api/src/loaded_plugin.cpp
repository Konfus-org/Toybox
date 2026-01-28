#include "tbx/plugin_api/loaded_plugin.h"
#include "tbx/debugging/macros.h"
#include "tbx/messages/dispatcher.h"
#include <string>
#include <utility>

namespace tbx
{
    LoadedPlugin::LoadedPlugin(
        PluginMeta meta_data,
        std::unique_ptr<SharedLibrary> plugin_library,
        std::unique_ptr<Plugin, PluginDeleter> plugin_instance)
        : meta(std::move(meta_data))
        , library(std::move(plugin_library))
        , instance(std::move(plugin_instance))
    {
        TBX_ASSERT(!meta.name.empty(), "LoadedPlugin requires a name.");
        TBX_ASSERT(!meta.version.empty(), "LoadedPlugin requires a version.");
        TBX_ASSERT(instance, "LoadedPlugin requires a plugin instance.");
        TBX_TRACE_INFO("Loaded plugin: {} v{}", meta.name, meta.version);
    }

    LoadedPlugin::~LoadedPlugin() noexcept
    {
        if (!is_valid())
        {
            return;
        }

        TBX_TRACE_INFO("Unloading plugin: {} v{}", meta.name, meta.version);
    }

    bool LoadedPlugin::is_valid() const
    {
        return instance != nullptr;
    }

    void LoadedPlugin::attach(
        Application& host,
        std::string_view host_name,
        IMessageDispatcher& dispatcher,
        IMessageHandlerRegistrar& registrar)
    {
        if (!is_valid())
        {
            TBX_ASSERT(false, "LoadedPlugin attach requires a valid plugin instance.");
            return;
        }

        TBX_ASSERT(
            !message_handler_token.is_valid(),
            "LoadedPlugin cannot attach while it still has a registered handler.");

        TBX_TRACE_INFO(
            "Attaching plugin: {} v{} to app {}",
            meta.name,
            meta.version,
            host_name);

        message_handler_token = registrar.add_handler(
            [plugin = instance.get()](Message& msg)
            {
                plugin->receive_message(msg);
            });

        instance->attach(host, dispatcher);
    }

    void LoadedPlugin::detach(
        Application& host,
        std::string_view host_name,
        IMessageHandlerRegistrar& registrar)
    {
        if (!is_valid())
        {
            return;
        }

        TBX_TRACE_INFO(
            "Detaching plugin: {} v{} from app {}",
            meta.name,
            meta.version,
            host_name);

        if (message_handler_token.is_valid())
        {
            registrar.remove_handler(message_handler_token);
            message_handler_token = {};
        }

        instance->detach();
    }

    std::string to_string(const LoadedPlugin& loaded)
    {
        return "Name=" + loaded.meta.name + ", Version=" + loaded.meta.version;
    }
}
