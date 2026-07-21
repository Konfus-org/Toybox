#pragma once
#include "tbx/core/api.h"
#include "tbx/assets/assets.h"
#include "tbx/assets/audio_clip.h"
#include "tbx/ecs/sandbox.h"

namespace tbx
{
    /// @brief
    /// Purpose: The ears: sounds spatialize relative to the first enabled listener's Transform.
    struct TBX_API AudioListener
    {
        float volume = 1.0f;
    };

    /// @brief
    /// Purpose: Makes a toy emit sound: a clip played at the toy's position. A Collider on the
    /// same toy gives the source its spatial extent (shared Shape vocabulary — bigger shapes
    /// attenuate more gently); without one it is a point source.
    struct TBX_API AudioSource
    {
        AssetHandle<AudioClip> clip = {};
        float volume = 1.0f;
        bool is_looping = false;
        bool is_playing = true;
    };
}

// The concrete audio boundary (see cmake/tbx_backend.cmake): audio/steamaudio/ implements it
// and its library types never escape that folder. State lives in the backend's cpp;
// initialization is lazy on first update.
namespace tbx::audio
{
    /// @brief
    /// Purpose: Advances audio one frame: mirrors listener/source toys into the spatializer
    /// (clips resolve through their handles) and keeps the output device fed. Called by
    /// tbx::run() every frame.
    TBX_API void update(Sandbox& sandbox, Assets& assets, float delta_time);

    /// @brief
    /// Purpose: Tears the audio engine down; the next update() starts fresh. run() calls this
    /// at shutdown, tests between scenarios.
    TBX_API void reset();
}
