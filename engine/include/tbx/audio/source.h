#pragma once
#include "tbx/api.h"
#include "tbx/assets/handle.h"
#include "tbx/audio/clip.h"
#include "tbx/ecs/block.h"
#include <utility>


namespace tbx::audio
{
    /// @brief
    /// Purpose: Makes a toy emit sound: a clip played at the toy's position. A Collider on the
    /// same toy gives the source its spatial extent (shared Shape vocabulary — bigger shapes
    /// attenuate more gently); without one it is a point source.
    struct TBX_API Source : ecs::Block
    {
        assets::Handle<Clip> clip = {};
        float volume = 1.0f;
        bool is_looping = false;
        bool is_playing = true;

        // Fluent setters — each returns *this for one-chain construction.
        Source& set_clip(assets::Handle<Clip> value) { clip = std::move(value); return *this; }
        Source& set_volume(float value) { volume = value; return *this; }
        Source& set_looping(bool value) { is_looping = value; return *this; }
        Source& set_playing(bool value) { is_playing = value; return *this; }
    };
}
