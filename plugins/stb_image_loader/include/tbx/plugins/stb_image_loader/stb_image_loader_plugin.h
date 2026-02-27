#pragma once
#include "tbx/assets/asset_manager.h"
#include "tbx/assets/messages.h"
#include "tbx/files/file_ops.h"
#include "tbx/plugin_api/plugin.h"
#include "tbx/plugin_api/plugin_export.h"
#include <filesystem>
#include <memory>

#ifndef TBX_STB_IMAGE_LOADER_PLUGIN_EXPORTS
    #define TBX_STB_IMAGE_LOADER_PLUGIN_EXPORTS 0
#endif

namespace stb_image_loader
{
    using namespace tbx;
    /// <summary>
    /// Purpose: Loads texture assets into Texture payloads using stb_image.
    /// </summary>
    /// <remarks>
    /// Ownership: Plugin lifetime is owned by the host; it keeps non-owning references to the host.
    /// Thread Safety: Handles asset messages on the dispatcher thread; no internal synchronization.
    /// </remarks>
    class TBX_PLUGIN_INCLUDE_API(TBX_STB_IMAGE_LOADER_PLUGIN_EXPORTS) StbImageLoaderPlugin final : public Plugin
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
        /// Thread Safety: Executes on the dispatcher thread; relies on Texture payload
        /// synchronization.
        /// </remarks>
        void on_recieve_message(Message& msg) override;

        /// <summary>
        /// Purpose: Overrides filesystem operations used by the loader.
        /// </summary>
        /// <remarks>
        /// Ownership: Shares ownership of file_ops with the caller.
        /// Thread Safety: Call before dispatching load messages.
        /// </remarks>
        void set_file_ops(std::shared_ptr<IFileOps> file_ops);

      private:
        void on_load_texture_request(LoadTextureRequest& request);

        std::shared_ptr<IFileOps> _file_ops = {};
    };
}
