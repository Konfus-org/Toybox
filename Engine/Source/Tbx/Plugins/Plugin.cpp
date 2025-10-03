#include "Tbx/PCH.h"
#include "Tbx/Plugins/Plugin.h"
#include "Tbx/Events/PluginEvents.h"
#include "Tbx/Debug/Debugging.h"

namespace Tbx
{
    Plugin::~Plugin()
    {
        NotifyUnloaded();
        _isInitialized = false;
        _self.reset();

        if (_library.IsValid())
        {
            _library.Unload();
        }
    }

    bool Plugin::IsInitialized() const
    {
        return _isInitialized;
    }

    const PluginMeta& Plugin::GetMeta() const
    {
        return _pluginInfo;
    }

    SharedLibrary& Plugin::GetLibrary()
    {
        return _library;
    }

    const SharedLibrary& Plugin::GetLibrary() const
    {
        return _library;
    }

    void Plugin::Bind(const Ref<Plugin>& self)
    {
        TBX_ASSERT(_self.expired(), "Plugin: self reference already bound for '{}'", _pluginInfo.Name);
        _self = self;
    }

    void Plugin::Initialize(const PluginMeta& pluginInfo, SharedLibrary library, Ref<EventBus> eventBus)
    {
        TBX_ASSERT(!_isInitialized, "Plugin: '{}' already initialized!", pluginInfo.Name);

        _pluginInfo = pluginInfo;
        _library = std::move(library);
        _eventBus = std::move(eventBus);
        _isInitialized = true;

        NotifyLoaded();
    }

    Ref<EventBus> Plugin::GetEventBus() const
    {
        return _eventBus;
    }

    void Plugin::NotifyLoaded() const
    {
        if (!_isInitialized || _eventBus == nullptr)
        {
            return;
        }

        _eventBus->Post(PluginLoadedEvent(_pluginInfo, _self));
    }

    void Plugin::NotifyUnloaded() const
    {
        if (!_isInitialized || _eventBus == nullptr)
        {
            return;
        }

        _eventBus->Post(PluginUnloadedEvent(_pluginInfo, _self));
    }

    StaticPlugin::~StaticPlugin()
    {
        NotifyUnloaded();
        _isInitialized = false;
    }

    bool StaticPlugin::IsInitialized() const
    {
        return _isInitialized;
    }

    const PluginMeta& StaticPlugin::GetMeta() const
    {
        return _pluginInfo;
    }

    void StaticPlugin::Initialize(const PluginMeta& pluginInfo, Ref<EventBus> eventBus)
    {
        TBX_ASSERT(!_isInitialized, "StaticPlugin: '{}' already initialized!", pluginInfo.Name);

        _pluginInfo = pluginInfo;
        _eventBus = std::move(eventBus);
        _isInitialized = true;

        NotifyLoaded();
    }

    Ref<EventBus> StaticPlugin::GetEventBus() const
    {
        return _eventBus;
    }

    void StaticPlugin::NotifyLoaded() const
    {
        if (!_isInitialized || _eventBus == nullptr)
        {
            return;
        }

        _eventBus->Post(PluginLoadedEvent(_pluginInfo, {}));
    }

    void StaticPlugin::NotifyUnloaded() const
    {
        if (!_isInitialized || _eventBus == nullptr)
        {
            return;
        }

        _eventBus->Post(PluginUnloadedEvent(_pluginInfo, {}));
    }
}
