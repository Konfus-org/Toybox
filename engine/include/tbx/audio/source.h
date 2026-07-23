#pragma once
#include "tbx/api.h"
#include "tbx/assets/handle.h"
#include "tbx/audio/clip.h"
#include "tbx/ecs/block.h"
#include "tbx/reflection/attributes.h"
#include <utility>


namespace tbx
{
    /// @brief
    /// Purpose: Makes a toy emit sound: a clip played at the toy's position. A Collider on the
    /// same toy gives the source its spatial extent (shared Shape vocabulary — bigger shapes
    /// attenuate more gently); without one it is a point source.
    struct TBX_SERIALIZABLE() TBX_EXPOSED_TO_SCRIPTING TBX_DLL_EXPORT AudioSource : Block
    {
        AssetHandle<AudioClip> clip = {};
        float volume = 1.0f;
        bool is_looping = false;
        bool is_playing = true;

        // Fluent setters — each returns *this for one-chain construction.
        AudioSource& set_clip(AssetHandle<AudioClip> value) { clip = std::move(value); return *this; }
        AudioSource& set_volume(float value) { volume = value; return *this; }
        AudioSource& set_looping(bool value) { is_looping = value; return *this; }
        AudioSource& set_playing(bool value) { is_playing = value; return *this; }
    };
}
