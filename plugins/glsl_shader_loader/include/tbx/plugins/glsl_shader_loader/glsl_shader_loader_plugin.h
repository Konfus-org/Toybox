#pragma once
#include "tbx/assets/manager.h"
#include "tbx/assets/requests.h"
#include "tbx/files/ops.h"
#include "tbx/plugin_api/plugin.h"
#include "tbx/plugin_api/plugin_export.h"
#include <filesystem>
#include <memory>

namespace glsl_shader_loader
{
    class TBX_PLUGIN_API GlslShaderLoaderPlugin final : public tbx::Plugin
    {
      public:
        void on_attach(tbx::ServiceProvider& service_provider) override;
        void on_detach() override;
        void on_recieve_message(tbx::Message& msg) override;

      private:
        void on_load_shader_program_request(tbx::LoadShaderRequest& request);

        tbx::AssetManager* _asset_manager = nullptr;
        std::filesystem::path _working_directory = {};
        std::unique_ptr<tbx::IFileOps> _file_ops = {};
    };
}
