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
        std::unique_ptr<Plugin, PluginDeleter> plugin_instance,
        IPluginHost& host)
        : meta(std::move(meta_data))
        , library(std::move(plugin_library))
        , instance(std::move(plugin_instance))
        , _host(&host)
    {
        TBX_ASSERT(!meta.name.empty(), "LoadedPlugin requires a name.");
        TBX_ASSERT(!meta.version.empty(), "LoadedPlugin requires a version.");
        TBX_ASSERT(instance, "LoadedPlugin requires a plugin instance.");
        TBX_TRACE_INFO("Loading plugin: {} v{}", meta.name, meta.version);
        IMessageHandlerRegistrar& registrar = host.get_message_registrar();
        message_handler_token = registrar.register_handler(
            [plugin = instance.get()](Message& msg)
            {
                plugin->receive_message(msg);
            });
        instance->attach(host);
    }

    LoadedPlugin::~LoadedPlugin() noexcept
    {
        if (!is_valid())
        {
            return;
        }

        TBX_TRACE_INFO("Unloading plugin: {}", meta.name);

        if (_host)
        {
            _host->get_message_queue().flush();
            if (message_handler_token.is_valid())
            {
                _host->get_message_registrar().deregister_handler(message_handler_token);
                message_handler_token = {};
            }
            instance->detach();
        }
    }

    bool LoadedPlugin::is_valid() const
    {
        return instance != nullptr;
    }

    std::string to_string(const LoadedPlugin& loaded)
    {
        return "Name=" + loaded.meta.name + ", Version=" + loaded.meta.version;
    }
}
