#pragma once
#include "tbx/utils/api.h"
#include "tbx/assets/assets.h"
#include "tbx/audio/audio_listener.h"
#include "tbx/audio/audio_source.h"
#include "tbx/ecs/sandbox.h"


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
