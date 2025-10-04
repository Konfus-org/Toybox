#include "Tbx/PCH.h"
#include "Tbx/Plugins/Plugin.h"
#include "Tbx/Debug/Tracers.h"

namespace Tbx
{
    Plugin::~Plugin()
    {
        if (_library != nullptr && _library->IsValid())
        {
            _library->Unload();
        }

        _isBound = false;
        _library.reset();
    }

    void Plugin::Bind(const PluginMeta& pluginInfo, ExclusiveRef<SharedLibrary> library)
    {
        if (_isBound)
        {
            TBX_TRACE_WARNING("Plugin: '{}' is already bound. Plugins cannot be rebound.", _pluginInfo.Name);
            return;
        }

        _pluginInfo = pluginInfo;
        _library = std::move(library);
        _isBound = true;
    }

    bool Plugin::IsBound() const
    {
        return _isBound;
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

    StaticPlugin::StaticPlugin(const PluginMeta& pluginInfo)
        : _pluginInfo(pluginInfo)
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
