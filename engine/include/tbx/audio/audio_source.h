#pragma once
#include "tbx/assets/asset_handle.h"
#include "tbx/audio/audio_clip.h"
#include "tbx/ecs/block.h"
#include "tbx/utils/api.h"

namespace tbx::audio
{
    /// @brief
    /// Purpose: Makes a toy emit sound: a clip played at the toy's position. A Collider on the
    /// same toy gives the source its spatial extent (shared Shape vocabulary — bigger shapes
    /// attenuate more gently); without one it is a point source.
    struct TBX_API AudioSource : ecs::Block
    {
        assets::AssetHandle<AudioClip> clip = {};
        float volume = 1.0f;
        bool is_looping = false;
        bool is_playing = true;
    };
}
