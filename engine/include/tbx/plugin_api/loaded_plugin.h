#pragma once
#include "tbx/memory/smart_pointers.h"
#include "tbx/os/shared_library.h"
#include "tbx/plugin_api/plugin_meta.h"
#include "tbx/tbx_api.h"
#include <functional>

namespace tbx
{
    class Plugin;
    using PluginDeleter = std::function<void(Plugin*)>;
    using PluginInstance = Scope<Plugin, PluginDeleter>;
    // Represents an owned plugin instance along with its loading metadata
    // and (optionally) the dynamic library used to load it.
    // Ownership: Owns `instance` and `library` (if any). Movable, non-copyable
    // by virtue of unique_ptr semantics.
    // Thread-safety: Not thread-safe; expected to be used by the main thread.
    struct TBX_API LoadedPlugin
    {
        PluginMeta meta;
        Scope<SharedLibrary> library; // only set for dynamic plugins
        PluginInstance instance;
    };
}
