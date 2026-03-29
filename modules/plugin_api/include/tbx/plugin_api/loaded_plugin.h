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
        LoadedPlugin() = default;
        LoadedPlugin(const LoadedPlugin&) = delete;
        LoadedPlugin& operator=(const LoadedPlugin&) = delete;
        LoadedPlugin(LoadedPlugin&&) noexcept = default;
        LoadedPlugin& operator=(LoadedPlugin&&) noexcept = default;
        LoadedPlugin(
            PluginMeta meta_data,
            std::unique_ptr<SharedLibrary> plugin_library,
            std::unique_ptr<Plugin, PluginDeleter> plugin_instance,
            IPluginHost& host);
        ~LoadedPlugin() noexcept;

        /// @brief Purpose: Reports whether the loaded plugin contains a valid instance.
        /// @details Ownership: Does not transfer ownership.
        /// Thread Safety: Not thread-safe.
        bool is_valid() const;

        PluginMeta meta;
        std::unique_ptr<SharedLibrary> library; // only set for dynamic plugins
        std::unique_ptr<Plugin, PluginDeleter> instance;

      private:
        Uuid message_handler_token = {}; // Host-owned message handler token.
        IPluginHost* _host = nullptr;
    };

    /// @brief Purpose: Formats a LoadedPlugin summary string.
    /// @details Ownership: Returns an owned std::string.
    /// Thread Safety: Stateless and safe for concurrent use.
    TBX_API std::string to_string(const LoadedPlugin& loaded);
}
