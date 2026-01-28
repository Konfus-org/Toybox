#pragma once
#include "tbx/common/uuid.h"
#include "tbx/plugin_api/plugin.h"
#include "tbx/plugin_api/plugin_meta.h"
#include "tbx/plugin_api/shared_library.h"
#include "tbx/tbx_api.h"
#include <functional>
#include <memory>
#include <string>

namespace tbx
{
    class IMessageHandlerRegistrar;
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

        /// <summary>Purpose: Creates a loaded plugin wrapper and logs the load event.</summary>
        /// <remarks>Ownership: Takes ownership of the plugin instance and library.
        /// Thread Safety: Not thread-safe.</remarks>
        LoadedPlugin(
            PluginMeta meta_data,
            std::unique_ptr<SharedLibrary> plugin_library,
            std::unique_ptr<Plugin, PluginDeleter> plugin_instance);

        /// <summary>Purpose: Releases plugin resources and logs unload details.</summary>
        /// <remarks>Ownership: Frees owned plugin instance/library.
        /// Thread Safety: Not thread-safe.</remarks>
        ~LoadedPlugin() noexcept;

        /// <summary>Purpose: Reports whether the loaded plugin contains a valid instance.</summary>
        /// <remarks>Ownership: Does not transfer ownership.
        /// Thread Safety: Not thread-safe.</remarks>
        bool is_valid() const;

        /// <summary>
        /// Purpose: Attaches the plugin to the host and registers its message handler.
        /// </summary>
        /// <remarks>
        /// Ownership: Does not own the host or handler registrar; stores a handler token.
        /// Thread Safety: Not thread-safe; expected to be used on the main thread.
        /// </remarks>
        void attach(
            Application& host,
            std::string_view host_name,
            IMessageHandlerRegistrar& registrar);

        /// <summary>
        /// Purpose: Detaches the plugin from the host and unregisters its message handler.
        /// </summary>
        /// <remarks>
        /// Ownership: Does not own the host or handler remover; clears the handler token.
        /// Thread Safety: Not thread-safe; expected to be used on the main thread.
        /// </remarks>
        void detach(
            Application& host,
            std::string_view host_name,
            IMessageHandlerRegistrar& registrar);

        PluginMeta meta;
        std::unique_ptr<SharedLibrary> library; // only set for dynamic plugins
        std::unique_ptr<Plugin, PluginDeleter> instance;
        Uuid message_handler_token = {}; // Application-owned message handler token.
    };

    /// <summary>Purpose: Formats a LoadedPlugin summary string.</summary>
    /// <remarks>Ownership: Returns an owned std::string.
    /// Thread Safety: Stateless and safe for concurrent use.</remarks>
    TBX_API std::string to_string(const LoadedPlugin& loaded);
}
