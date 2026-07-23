#pragma once
#include "tbx/audio/audio.h"

namespace tbx::internal
{
    /// @brief
    /// Purpose: Advances audio one frame: mirrors the sandbox's listener/source toys into
    /// the spatializer (clips resolve through the asset states) and keeps the output device
    /// fed. Called by tbx::run() every frame.
    void update_audio(
        AudioState& audio,
        Sandbox& sandbox,
        AssetsState& assets,
        EventsState& events,
        float delta_time);
}
