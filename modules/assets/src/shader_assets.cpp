#include "tbx/assets/shader_assets.h"
#include "tbx/assets/messages.h"
#include "tbx/debugging/macros.h"
#include <memory>
#include <vector>

namespace tbx
{
    static std::shared_ptr<Shader> create_shader_data()
    {
        auto vertex_shader = ShaderSource(
            "#version 450 core\n"
            "layout(location = 0) in vec3 a_position;\n"
            "uniform mat4 u_view_proj = mat4(1.0);\n"
            "uniform mat4 u_model = mat4(1.0);\n"
            "void main()\n"
            "{\n"
            "    gl_Position = u_view_proj * (u_model * vec4(a_position, 1.0));\n"
            "}\n",
            ShaderType::VERTEX);

        auto fragment_shader = ShaderSource(
            "#version 450 core\n"
            "layout(location = 0) out vec4 o_color;\n"
            "void main()\n"
            "{\n"
            "    o_color = vec4(1.0, 0.0, 1.0, 1.0);\n"
            "}\n",
            ShaderType::FRAGMENT);

        auto sources = std::vector<ShaderSource>();
        sources.push_back(std::move(vertex_shader));
        sources.push_back(std::move(fragment_shader));
        return std::make_shared<Shader>(std::move(sources));
    }

    AssetPromise<Shader> load_shader_async(const std::filesystem::path& asset_path)
    {
        auto asset = create_shader_data();
        auto* dispatcher = get_global_dispatcher();
        if (!dispatcher)
        {
            AssetPromise<Shader> result = {};
            result.asset = asset;
            warn_missing_dispatcher("load a shader asynchronously");
            result.promise = make_missing_dispatcher_future("load a shader asynchronously");
            return result;
        }

        LoadShaderRequest message(asset_path, asset.get());
        message.not_handled_behavior = MessageNotHandledBehavior::WARN;
        auto future = dispatcher->post(message);
        AssetPromise<Shader> result = {};
        result.asset = asset;
        result.promise = future;
        return result;
    }

    std::shared_ptr<Shader> load_shader(const std::filesystem::path& asset_path)
    {
        auto asset = create_shader_data();
        auto* dispatcher = get_global_dispatcher();
        if (!dispatcher)
        {
            warn_missing_dispatcher("load a shader synchronously");
            return asset;
        }

        LoadShaderRequest message(asset_path, asset.get());
        message.not_handled_behavior = MessageNotHandledBehavior::WARN;
        auto result = dispatcher->send(message);
        if (!result.succeeded())
        {
            TBX_TRACE_WARNING(
                "Shader load request failed for '{}': {}. Using fallback pink shader.",
                asset_path.string(),
                result.get_report());
        }
        return asset;
    }
}
