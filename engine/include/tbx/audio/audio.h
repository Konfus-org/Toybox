#pragma once
#include "tbx/api.h"
#include "tbx/assets/assets.h"
#include "tbx/ecs/sandbox.h"
#include "tbx/events/events.h"
#include <atomic>
#include <memory>

// The concrete audio boundary (see cmake/tbx_backend.cmake): audio/steamaudio/ implements
// it and its library types never escape that folder. The state is runtime.audio; the master
// volume is a plain (atomic) field.
namespace tbx
{
    /// @brief
    /// Purpose: The audio module's state, held by value on the Runtime. The spatializer and
    /// output device live behind the backend seam (audio/steamaudio/ defines Backend; library
    /// types never escape that folder) and are built lazily on the first update.
    struct TBX_DLL_EXPORT AudioState
    {
        AudioState();
        ~AudioState();

        AudioState(const AudioState&) = delete;
        AudioState& operator=(const AudioState&) = delete;

        // Written on the main thread, read on the audio thread.
        std::atomic<float> master_volume = 1.0f;

        struct Backend; // defined by the audio backend's .cpp
        std::unique_ptr<Backend> backend;
    };

    /// @brief
    /// Purpose: Advances audio one frame: mirrors the sandbox's listener/source toys into
    /// the spatializer (clips resolve through the asset states) and keeps the output device
    /// fed. Called by tbx::run() every frame.
    TBX_DLL_EXPORT void update_audio(
        AudioState& audio,
        Sandbox& sandbox,
        AssetsState& assets,
        EventsState& events,
        float delta_time);
}
