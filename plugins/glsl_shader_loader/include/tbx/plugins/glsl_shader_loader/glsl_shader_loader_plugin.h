#pragma once
#include "tbx/assets/asset_requests.h"
#include "tbx/files/file_ops.h"
#include "tbx/plugin_api/plugin.h"
#include "tbx/plugin_api/plugin_export.h"
#include <filesystem>
#include <memory>

namespace glsl_shader_loader
{
    class TBX_PLUGIN_API GlslShaderLoaderPlugin final : public tbx::Plugin
    {
      public:
        void on_attach(tbx::IPluginHost& host) override;
        void on_detach() override;
        void on_recieve_message(tbx::Message& msg) override;

      private:
        void on_load_shader_program_request(tbx::LoadShaderRequest& request);

        std::filesystem::path _working_directory = {};
        std::unique_ptr<tbx::IFileOps> _file_ops = {};
    };
}
