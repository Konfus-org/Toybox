#pragma once
#include "tbx/assets/messages.h"
#include "tbx/files/file_ops.h"
#include "tbx/plugin_api/plugin.h"
#include "tbx/plugin_api/plugin_export.h"
#include <memory>

namespace stb_image_loader
{
    /// <summary>
    /// Purpose: Loads texture assets into tbx::Texture payloads using stb_image.
    /// </summary>
    /// <remarks>
    /// Ownership: tbx::Plugin lifetime is owned by the host; it keeps non-owning references to the
    /// host. Thread Safety: Handles asset messages on the dispatcher thread; no internal
    /// synchronization.
    /// </remarks>
    class TBX_PLUGIN_API StbImageLoaderPlugin final : public tbx::Plugin
    {
      public:
        /// <summary>
        /// Purpose: Captures host references needed for asset resolution.
        /// </summary>
        /// <remarks>
        /// Ownership: Does not take ownership of the host.
        /// Thread Safety: Called on the main thread during plugin attach.
        /// </remarks>
        void on_attach(tbx::IPluginHost& host) override;

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
        /// Thread Safety: Executes on the dispatcher thread; relies on tbx::Texture payload
        /// synchronization.
        /// </remarks>
        void on_recieve_message(tbx::Message& msg) override;

      private:
        void on_load_texture_request(tbx::LoadTextureRequest& request) const;

        std::unique_ptr<tbx::IFileOps> _file_ops = {};
    };
}
