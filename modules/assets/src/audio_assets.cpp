#include "tbx/assets/audio_assets.h"
#include "tbx/assets/messages.h"
#include <memory>

namespace tbx
{
    static std::unique_ptr<AudioClip> create_audio_data(
        const std::shared_ptr<AudioClip>& default_data)
    {
        if (default_data)
        {
            return std::make_unique<AudioClip>(*default_data);
        }

        return std::make_unique<AudioClip>();
    }

    static std::shared_ptr<Asset<AudioClip>> create_audio_asset(
        const std::shared_ptr<AudioClip>& default_data)
    {
        return std::make_shared<Asset<AudioClip>>(create_audio_data(default_data));
    }

    AssetPromise<AudioClip> load_audio_async(
        const std::filesystem::path& asset_path,
        const std::shared_ptr<AudioClip>& default_data)
    {
        auto asset = create_audio_asset(default_data);
        auto* dispatcher = get_global_dispatcher();
        if (!dispatcher)
        {
            throw_missing_dispatcher("load audio asynchronously");
        }

        auto future = dispatcher->post<LoadAudioRequest>(
            asset_path,
            asset.get());
        AssetPromise<AudioClip> result = {};
        result.asset = asset;
        result.promise = future;
        return result;
    }

    std::shared_ptr<Asset<AudioClip>> load_audio(
        const std::filesystem::path& asset_path,
        const std::shared_ptr<AudioClip>& default_data)
    {
        auto asset = create_audio_asset(default_data);
        auto* dispatcher = get_global_dispatcher();
        if (!dispatcher)
        {
            throw_missing_dispatcher("load audio synchronously");
        }

        LoadAudioRequest message(
            asset_path,
            asset.get());
        dispatcher->send(message);
        return asset;
    }
}
