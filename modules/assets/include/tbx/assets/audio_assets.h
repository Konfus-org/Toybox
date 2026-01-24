#pragma once
#include "tbx/assets/assets.h"
#include "tbx/common/int.h"
#include "tbx/tbx_api.h"
#include <memory>
#include <vector>

namespace tbx
{
    /// <summary>
    /// Purpose: Represents raw audio sample data for asset loading.
    /// </summary>
    /// <remarks>
    /// Ownership: Instances own their sample buffers.
    /// Thread Safety: Safe to move across threads; synchronize shared mutation externally.
    /// </remarks>
    struct TBX_API AudioClip
    {
        uint32 sample_rate = 44100;
        uint16 channels = 2;
        std::vector<float> samples = {};
    };

    /// <summary>
    /// Purpose: Begins loading audio asynchronously via the global message dispatcher.
    /// </summary>
    /// <remarks>
    /// Ownership: Returns an AssetPromise that shares ownership of the audio wrapper with
    /// the caller. The payload is destroyed when the final shared reference is released.
    /// Thread Safety: Safe to call concurrently provided the global dispatcher is thread-safe.
    /// </remarks>
    TBX_API AssetPromise<AudioClip> load_audio_async(
        const std::filesystem::path& asset_path,
        const std::shared_ptr<AudioClip>& default_data = {});

    /// <summary>
    /// Purpose: Loads audio synchronously via the global message dispatcher.
    /// </summary>
    /// <remarks>
    /// Ownership: Returns a shared audio wrapper owned by the caller. The payload is destroyed
    /// when the final shared reference is released.
    /// Thread Safety: Safe to call concurrently provided the global dispatcher is thread-safe.
    /// </remarks>
    TBX_API std::shared_ptr<Asset<AudioClip>> load_audio(
        const std::filesystem::path& asset_path,
        const std::shared_ptr<AudioClip>& default_data = {});
}
