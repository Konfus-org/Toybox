#pragma once
#include "tbx/interfaces/graphics_backend.h"

namespace opengl_rendering
{
    /// @brief
    /// Purpose: Defines the backend-ready action needed for one bind group entry.
    enum class OpenGlBindEntryType
    {
        VERTEX_BUFFER,
        INDEX_BUFFER,
        UNIFORM_BUFFER,
        STORAGE_BUFFER,
        SAMPLED_TEXTURE,
        SAMPLER,
    };

    /// @brief
    /// Purpose: Stores one bind group entry after type resolution.
    struct OpenGlBindEntry
    {
        OpenGlBindEntryType type = OpenGlBindEntryType::UNIFORM_BUFFER;
        uint32 slot = 0U;
        tbx::GpuId resource = tbx::INVALID_GPU_ID;
        uint64 offset = 0U;
        uint64 range = 0U;
    };
}
