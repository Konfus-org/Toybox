#pragma once
#include "tbx/assets/asset_manager.h"
#include "tbx/assets/messages.h"
#include "tbx/plugin_api/plugin.h"
#include <filesystem>

namespace tbx::plugins
{
    /// <summary>
    /// Purpose: Loads texture assets into Texture payloads using stb_image.
    /// </summary>
    /// <remarks>
    /// Ownership: Plugin lifetime is owned by the host; it keeps non-owning references to the host.
    /// Thread Safety: Handles asset messages on the dispatcher thread; no internal synchronization.
    /// </remarks>
    class StbImageLoaderPlugin final : public Plugin
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
        /// Purpose: Receives texture load requests and dispatches stb image loads.
        /// </summary>
        /// <remarks>
        /// Ownership: Does not take ownership of messages or asset payloads.
        /// Thread Safety: Executes on the dispatcher thread; relies on Texture payload synchronization.
        /// </remarks>
        void on_recieve_message(Message& msg) override;

      private:
        void on_load_texture_request(LoadTextureRequest& request);
        std::filesystem::path resolve_asset_path(const std::filesystem::path& path) const;

        AssetManager* _asset_manager = nullptr;
    };
}
