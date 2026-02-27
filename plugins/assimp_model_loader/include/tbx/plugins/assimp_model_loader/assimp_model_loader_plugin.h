#pragma once
#include "tbx/assets/asset_manager.h"
#include "tbx/assets/messages.h"
#include "tbx/plugin_api/plugin.h"
#include "tbx/plugin_api/plugin_export.h"
#include <filesystem>

#ifndef TBX_ASSIMP_MODEL_LOADER_PLUGIN_EXPORTS
    #define TBX_ASSIMP_MODEL_LOADER_PLUGIN_EXPORTS 0
#endif

namespace assimp_model_loader
{
    using namespace tbx;
    /// <summary>
    /// Purpose: Loads model assets into Model payloads using Assimp.
    /// </summary>
    /// <remarks>
    /// Ownership: Plugin lifetime is owned by the host; it keeps non-owning references to the host.
    /// Thread Safety: Handles asset messages on the dispatcher thread; no internal synchronization.
    /// </remarks>
    // Plugin implementation that bridges Assimp model loading to the Toybox asset system.
    class TBX_PLUGIN_INCLUDE_API(TBX_ASSIMP_MODEL_LOADER_PLUGIN_EXPORTS) AssimpModelLoaderPlugin final : public Plugin
    {
      public:
        /// <summary>
        /// Purpose: Captures host references needed for asset resolution.
        /// </summary>
        /// <remarks>
        /// Ownership: Does not take ownership of the host.
        /// Thread Safety: Called on the main thread during plugin attach.
        /// </remarks>
        void on_attach(IPluginHost& host) override;

        /// <summary>
        /// Purpose: Releases any cached host references.
        /// </summary>
        /// <remarks>
        /// Ownership: Does not destroy host resources.
        /// Thread Safety: Called on the main thread during plugin detach.
        /// </remarks>
        void on_detach() override;

        /// <summary>
        /// Purpose: Receives model load requests and dispatches Assimp imports.
        /// </summary>
        /// <remarks>
        /// Ownership: Does not take ownership of messages or asset payloads.
        /// Thread Safety: Executes on the dispatcher thread; relies on Model payload
        /// synchronization.
        /// </remarks>
        void on_recieve_message(Message& msg) override;

      private:
        // Handles a model load request message.
        void on_load_model_request(LoadModelRequest& request);

        // Resolves a request-relative path against the asset search roots when needed.
        std::filesystem::path resolve_asset_path(const std::filesystem::path& path) const;
    };
}
