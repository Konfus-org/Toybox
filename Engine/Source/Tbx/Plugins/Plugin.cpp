#include "Tbx/PCH.h"
#include "Tbx/Plugins/Plugin.h"
#include "Tbx/Events/PluginEvents.h"
#include "Tbx/Debug/Debugging.h"

namespace Tbx
{
    Plugin::Plugin(Ref<EventBus> eventBus)
        : _eventBus(std::move(eventBus))
    {
    }

    Plugin::~Plugin()
    {
        if (_library != nullptr && _library->IsValid())
        {
            _library->Unload();
        }

        _isBound = false;
        _library.reset();
    }

    bool Plugin::IsBound() const
    {
        return _isBound;
    }

    void Plugin::Bind(const PluginMeta& pluginInfo, ExclusiveRef<SharedLibrary> library, WeakRef<Plugin> self)
    {
        if (_isBound)
        {
            TBX_TRACE_WARN("Plugin: '{}' is already bound. Plugins cannot be rebound.", _pluginInfo.Name);
            return;
        }

        _pluginInfo = pluginInfo;
        _library = std::move(library);
        _isBound = true;

        if (_eventBus != nullptr)
        {
            _eventBus->Post(PluginLoadedEvent(_pluginInfo, std::move(self)));
        }
    }

    const PluginMeta& Plugin::GetMeta() const
    {
        return _pluginInfo;
    }

    const SharedLibrary* Plugin::GetLibrary() const
    {
        return _library.get();
    }

    void Plugin::ListSymbols() const
    {
        if (_library == nullptr)
        {
            return;
        }

        _library->ListSymbols();
    }

    Ref<EventBus> Plugin::GetEventBus() const
    {
        return _eventBus;
    }

    StaticPlugin::StaticPlugin(const PluginMeta& pluginInfo, Ref<EventBus> eventBus)
        : _pluginInfo(pluginInfo)
        , _eventBus(std::move(eventBus))
    {
    }

    StaticPlugin::~StaticPlugin()
    {
    }

    const PluginMeta& StaticPlugin::GetMeta() const
    {
        return _pluginInfo;
    }

    Ref<EventBus> StaticPlugin::GetEventBus() const
    {
        return _eventBus;
    }
}
