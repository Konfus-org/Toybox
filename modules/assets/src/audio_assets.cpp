#include "tbx/assets/audio_assets.h"
#include "tbx/assets/messages.h"

namespace tbx
{
    static void destroy_audio_payload(AudioClip* payload)
    {
        delete payload;
    }

    static std::shared_ptr<AudioClip> create_audio_payload(
        const std::shared_ptr<AudioClip>& default_data)
    {
        if (default_data)
        {
            return std::shared_ptr<AudioClip>(
                new AudioClip(*default_data),
                &destroy_audio_payload);
        }

        return std::shared_ptr<AudioClip>(
            new AudioClip(),
            &destroy_audio_payload);
    }

    AssetPromise<AudioClip> load_audio_async(
        const std::filesystem::path& asset_path,
        const std::shared_ptr<AudioClip>& default_data)
    {
        auto asset = create_audio_payload(default_data);
        auto* dispatcher = get_global_dispatcher();
        if (!dispatcher)
        {
            AssetPromise<AudioClip> result = {};
            result.asset = asset;
            result.promise = make_missing_dispatcher_future("load audio asynchronously");
            return result;
        }

        auto future = dispatcher->post<LoadAudioRequest>(
            asset_path,
            asset.get());
        AssetPromise<AudioClip> result = {};
        result.asset = asset;
        result.promise = future;
        return result;
    }

    std::shared_ptr<AudioClip> load_audio(
        const std::filesystem::path& asset_path,
        const std::shared_ptr<AudioClip>& default_data)
    {
        auto asset = create_audio_payload(default_data);
        auto* dispatcher = get_global_dispatcher();
        if (!dispatcher)
        {
            dispatcher_missing_result("load audio synchronously");
            return asset;
        }

        LoadAudioRequest message(
            asset_path,
            asset.get());
        dispatcher->send(message);
        return asset;
    }
}
