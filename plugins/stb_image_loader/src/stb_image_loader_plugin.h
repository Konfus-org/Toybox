#pragma once
#include "tbx/interfaces/file_ops.h"
#include "tbx/interfaces/plugin.h"
#include "tbx/systems/assets/serialization_registry.h"
#include "tbx/systems/plugin_api/plugin_export.h"
#include "tbx/systems/scripting/service_ref.h"

namespace stb_image_loader
{
    /// @brief
    /// Purpose: Loads texture assets into tbx::Texture payloads using stb_image.
    /// @details
    /// Ownership: tbx::Plugin lifetime is owned by the host; it keeps non-owning references to the
    /// host. Thread Safety: Handles asset messages on the dispatcher thread; no internal
    /// synchronization.
    [[tbx::plugin(
        name = "StbImageLoaderPlugin",
        version = "1.0.0",
        category = tbx::PluginCategory::DEFAULT)]];
    class TBX_PLUGIN_API StbImageLoaderPlugin final : public tbx::Plugin
    {
      public:
        void on_attach() override;
        void on_detach() override;

      public:
        [[tbx::inject]]
        tbx::ServiceRef<tbx::IFileOps> file_ops = {};
        [[tbx::inject]]
        tbx::ServiceRef<tbx::SerializationRegistry> serialization_registry = {};

      private:
        tbx::Result read_texture(
            const std::filesystem::path& asset_path,
            const tbx::TextureLoadParameters& parameters,
            const tbx::AssetLoadMetadata& metadata,
            tbx::Texture& texture) const;
    };
}
