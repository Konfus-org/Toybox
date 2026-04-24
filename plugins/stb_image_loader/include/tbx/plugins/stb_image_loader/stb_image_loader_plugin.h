#pragma once
#include "tbx/interfaces/file_ops.h"
#include "tbx/interfaces/plugin.h"
#include "tbx/systems/assets/serialization_registry.h"
#include "tbx/systems/plugin_api/plugin_export.h"
#include <functional>
#include <memory>
#include <optional>

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

      private:
        std::shared_ptr<tbx::Texture> read_texture(
            const std::filesystem::path& asset_path,
            const tbx::TextureLoadParameters& parameters) const;

        std::unique_ptr<tbx::IFileOps> _file_ops = {};
        std::optional<std::reference_wrapper<tbx::SerializationRegistry>> _serialization_registry =
            std::nullopt;
    };
}
