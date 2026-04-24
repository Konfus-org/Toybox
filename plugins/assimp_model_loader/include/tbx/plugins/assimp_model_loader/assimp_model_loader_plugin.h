#pragma once
#include "tbx/interfaces/plugin.h"
#include "tbx/systems/assets/serialization_registry.h"
#include "tbx/systems/plugin_api/plugin_export.h"
#include <functional>
#include <optional>

namespace assimp_model_loader
{
    class TBX_PLUGIN_API AssimpModelLoaderPlugin final : public tbx::Plugin
    {
      public:
        void on_attach(tbx::ServiceProvider& service_provider) override;
        void on_detach() override;

      private:
        static std::shared_ptr<tbx::Model> read_model(
            const std::filesystem::path& asset_path,
            const tbx::ModelLoadParameters& parameters);

        std::optional<std::reference_wrapper<tbx::SerializationRegistry>> _serialization_registry =
            std::nullopt;
    };
}
