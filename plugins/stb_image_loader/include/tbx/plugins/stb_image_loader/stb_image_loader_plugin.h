#pragma once
#include "tbx/interfaces/file_ops.h"
#include "tbx/interfaces/plugin.h"
#include "tbx/systems/assets/messages.h"
#include "tbx/systems/plugin_api/plugin_export.h"
#include <memory>


namespace stb_image_loader
{
    /// @brief
    /// Purpose: Loads texture assets into tbx::Texture payloads using stb_image.
    /// @details
    /// Ownership: tbx::Plugin lifetime is owned by the host; it keeps non-owning references to the
    /// host. Thread Safety: Handles asset messages on the dispatcher thread; no internal
    /// synchronization.
    class TBX_PLUGIN_API StbImageLoaderPlugin final : public tbx::Plugin
    {
      public:
        void on_attach(tbx::ServiceProvider& service_provider) override;
        void on_detach() override;

        /// @brief
        /// Purpose: Receives texture load requests and dispatches stb image loads.
        /// @details
        /// Ownership: Does not take ownership of messages or asset payloads.
        /// Thread Safety: Executes on the dispatcher thread; relies on tbx::Texture payload
        /// synchronization.
        void on_recieve_message(tbx::Message& msg) override;

      private:
        void on_load_texture_request(tbx::LoadTextureRequest& request) const;

        std::unique_ptr<tbx::IFileOps> _file_ops = {};
    };
}
