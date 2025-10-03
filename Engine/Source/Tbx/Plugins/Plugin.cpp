#include "Tbx/PCH.h"
#include "Tbx/Plugins/Plugin.h"
#include "Tbx/Debug/Debugging.h"

namespace Tbx
{
    Plugin::~Plugin()
    {
        if (_ownsLibrary && _library.IsValid())
        {
            _library.Unload();
        }
    }

    bool Plugin::IsInitialized() const
    {
        return _isInitialized;
    }

    bool Plugin::IsStatic() const
    {
        return _pluginInfo.IsStatic;
    }

    bool Plugin::HasLibrary() const
    {
        return _ownsLibrary && _library.IsValid();
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

    void Plugin::Initialize(const PluginMeta& pluginInfo)
    {
        TBX_ASSERT(!_isInitialized, "Plugin: '{}' already initialized!", pluginInfo.Name);
        _pluginInfo = pluginInfo;
        _isInitialized = true;
        _ownsLibrary = !pluginInfo.IsStatic;
    }

    void Plugin::Initialize(const PluginMeta& pluginInfo, const SharedLibrary& library)
    {
        Initialize(pluginInfo);
        _library = library;
    }
}
