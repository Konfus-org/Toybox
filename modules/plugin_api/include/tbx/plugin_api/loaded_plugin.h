#pragma once
#include "tbx/common/uuid.h"
#include "tbx/plugin_api/plugin.h"
#include "tbx/plugin_api/plugin_host.h"
#include "tbx/plugin_api/plugin_meta.h"
#include "tbx/plugin_api/shared_library.h"
#include "tbx/tbx_api.h"
#include <functional>
#include <memory>
#include <string>

namespace tbx
{
    using PluginDeleter = std::function<void(Plugin*)>;
    // Represents an owned plugin instance along with its loading metadata
    // and (optionally) the dynamic library used to load it.
    // Ownership: Owns `instance` and `library` (if any). Movable, non-copyable
    // by virtue of unique_ptr semantics.
    // Thread-safety: Not thread-safe; expected to be used by the main thread.
    struct TBX_API LoadedPlugin
    {
        /// <summary>Purpose: Creates an invalid placeholder loaded plugin.</summary>
        /// <remarks>Ownership: Does not own any plugin resources.
        /// Thread Safety: Not thread-safe.</remarks>
        LoadedPlugin() = default;
        LoadedPlugin(const LoadedPlugin&) = delete;
        LoadedPlugin& operator=(const LoadedPlugin&) = delete;
        LoadedPlugin(LoadedPlugin&&) noexcept = default;
        LoadedPlugin& operator=(LoadedPlugin&&) noexcept = default;

        /// <summary>Purpose: Creates a loaded plugin wrapper and attaches it to the host.</summary>
        /// <remarks>Ownership: Takes ownership of the plugin instance and library.
        /// The host reference must outlive this LoadedPlugin.
        /// Thread Safety: Not thread-safe.</remarks>
        LoadedPlugin(
            PluginMeta meta_data,
            std::unique_ptr<SharedLibrary> plugin_library,
            std::unique_ptr<Plugin, PluginDeleter> plugin_instance,
            IPluginHost& host);

        /// <summary>Purpose: Releases plugin resources and logs unload details.</summary>
        /// <remarks>Ownership: Frees owned plugin instance/library.
        /// Thread Safety: Not thread-safe.</remarks>
        ~LoadedPlugin() noexcept;

        /// <summary>Purpose: Reports whether the loaded plugin contains a valid instance.</summary>
        /// <remarks>Ownership: Does not transfer ownership.
        /// Thread Safety: Not thread-safe.</remarks>
        bool is_valid() const;

        PluginMeta meta;
        std::unique_ptr<SharedLibrary> library; // only set for dynamic plugins
        std::unique_ptr<Plugin, PluginDeleter> instance;

      private:
        Uuid message_handler_token = {}; // Host-owned message handler token.
        IPluginHost* _host = nullptr;
    };

    /// <summary>Purpose: Formats a LoadedPlugin summary string.</summary>
    /// <remarks>Ownership: Returns an owned std::string.
    /// Thread Safety: Stateless and safe for concurrent use.</remarks>
    TBX_API std::string to_string(const LoadedPlugin& loaded);
}
