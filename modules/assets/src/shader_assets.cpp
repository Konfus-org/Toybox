#include "tbx/assets/shader_assets.h"
#include "tbx/assets/messages.h"
#include <memory>

namespace tbx
{
    static std::shared_ptr<Shader> create_shader_data()
    {
        // TODO: return bright pink shader
        return std::make_shared<Shader>();
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
        message.not_handled_behavior = MessageNotHandledBehavior::Warn;
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
        message.not_handled_behavior = MessageNotHandledBehavior::Warn;
        dispatcher->send(message);
        return asset;
    }
}
