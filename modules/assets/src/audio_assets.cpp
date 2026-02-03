#include "tbx/assets/audio_assets.h"
#include "tbx/assets/messages.h"
#include <memory>

namespace tbx
{
    static std::shared_ptr<AudioClip> create_audio_data(
        const std::shared_ptr<AudioClip>& default_data)
    {
        if (default_data)
        {
            return std::make_shared<AudioClip>(*default_data);
        }

        return std::make_shared<AudioClip>();
    }

    AssetPromise<AudioClip> load_audio_async(
        const std::filesystem::path& asset_path,
        const std::shared_ptr<AudioClip>& default_data)
    {
        auto asset = create_audio_data(default_data);
        auto* dispatcher = get_global_dispatcher();
        if (!dispatcher)
        {
            AssetPromise<AudioClip> result = {};
            result.asset = asset;
            warn_missing_dispatcher("load audio asynchronously");
            result.promise = make_missing_dispatcher_future("load audio asynchronously");
            return result;
        }

        LoadAudioRequest message(asset_path, asset.get());
        message.not_handled_behavior = MessageNotHandledBehavior::Warn;
        auto future = dispatcher->post(message);
        AssetPromise<AudioClip> result = {};
        result.asset = asset;
        result.promise = future;
        return result;
    }

    std::shared_ptr<AudioClip> load_audio(
        const std::filesystem::path& asset_path,
        const std::shared_ptr<AudioClip>& default_data)
    {
        auto asset = create_audio_data(default_data);
        auto* dispatcher = get_global_dispatcher();
        if (!dispatcher)
        {
            warn_missing_dispatcher("load audio synchronously");
            return asset;
        }

        LoadAudioRequest message(asset_path, asset.get());
        message.not_handled_behavior = MessageNotHandledBehavior::Warn;
        dispatcher->send(message);
        return asset;
    }
}
