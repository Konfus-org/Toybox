#pragma once
#include "tbx/interfaces/file_ops.h"
#include "tbx/interfaces/plugin.h"
#include "tbx/systems/assets/serialization_registry.h"
#include "tbx/systems/plugin_api/plugin_export.h"
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>

namespace mat_material_loader
{
    /// @brief
    /// Purpose: Loads material assets from .mat JSON files.
    /// @details
    /// Ownership: tbx::Plugin lifetime is owned by the host; it keeps non-owning references to the
    /// host. Thread Safety: Handles asset messages on the dispatcher thread; no internal
    /// synchronization.
    class TBX_PLUGIN_API MatMaterialLoaderPlugin final : public tbx::Plugin
    {
      public:
        void on_attach(tbx::ServiceProvider& service_provider) override;
        void on_detach() override;

      private:
        std::shared_ptr<tbx::Material> read_material(
            const std::filesystem::path& asset_path,
            const tbx::MaterialLoadParameters& parameters);

        std::filesystem::path _working_directory = {};
        std::shared_ptr<tbx::IFileOps> _file_ops = {};
        std::optional<std::reference_wrapper<tbx::SerializationRegistry>> _serialization_registry =
            std::nullopt;
    };
}
