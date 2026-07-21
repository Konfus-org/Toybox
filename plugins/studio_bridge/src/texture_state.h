#pragma once
#include "tbx/interfaces/graphics_backend.h"
#include "tbx/types/typedefs.h"
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace tbx::studio_bridge
{
    /// @brief One editor-uploaded texture: the CPU pixels (RGBA8, row-major, top-left origin) and
    /// the lazily-created GPU resource (uploaded by the sprite pass on the render lane the first
    /// time a sprite references it).
    struct EditorTexture
    {
        uint32 width = 0U;
        uint32 height = 0U;
        std::vector<uint8> pixels = {};
        tbx::GpuId gpu = tbx::INVALID_GPU_ID;
    };

    /// @brief
    /// Purpose: The editor-supplied textures (texture.upload), keyed by the id the upload replied.
    /// Shared (by shared_ptr) with the draw lane's sprite pass, which uploads entries to the GPU on
    /// demand and keeps the table alive across an in-flight frame.
    /// @details
    /// Ownership: Owned by the plugin; the pass callback co-owns through its capture. Thread
    /// Safety: every access holds `mutex` — uploads happen on the main thread (RPC dispatch), GPU
    /// realization + lookups on the render lane.
    struct EditorTextureTable
    {
        std::mutex mutex = {};
        std::unordered_map<uint64, EditorTexture> textures = {};
        uint64 next_id = 1U;
    };
}
