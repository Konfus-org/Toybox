#include "tbx/assets/shader_assets.h"
#include "tbx/assets/messages.h"
#include <memory>

namespace tbx
{
    static std::shared_ptr<ShaderProgram> create_shader_program_data(
        const std::shared_ptr<ShaderProgram>& default_data)
    {
        if (default_data)
        {
            return std::make_shared<ShaderProgram>(*default_data);
        }

        return std::make_shared<ShaderProgram>();
    }

    AssetPromise<ShaderProgram> load_shader_program_async(
        const std::filesystem::path& asset_path,
        const std::shared_ptr<ShaderProgram>& default_data)
    {
        auto asset = create_shader_program_data(default_data);
        auto* dispatcher = get_global_dispatcher();
        if (!dispatcher)
        {
            return {};
        }

        auto future = dispatcher->post<LoadShaderProgramRequest>(
            asset_path,
            asset.get());
        AssetPromise<ShaderProgram> result = {};
        result.asset = asset;
        result.promise = future;
        return result;
    }

    std::shared_ptr<ShaderProgram> load_shader_program(
        const std::filesystem::path& asset_path,
        const std::shared_ptr<ShaderProgram>& default_data)
    {
        auto asset = create_shader_program_data(default_data);
        auto* dispatcher = get_global_dispatcher();
        if (!dispatcher)
        {
            return {};
        }

        LoadShaderProgramRequest message(
            asset_path,
            asset.get());
        dispatcher->send(message);
        return asset;
    }
}
