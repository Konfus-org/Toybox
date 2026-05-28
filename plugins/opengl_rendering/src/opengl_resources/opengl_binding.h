#pragma once

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
        STORAGE_TEXTURE,
        SAMPLER,
    };

    /// @brief
    /// Purpose: Stores one bind group entry after type resolution.
    struct OpenGlBindEntry
    {
        OpenGlBindEntryType type = OpenGlBindEntryType::UNIFORM_BUFFER;
        uint32 slot = 0U;
        tbx::Uuid resource = {};
        uint64 offset = 0U;
        uint64 range = 0U;
    };

    /// @brief
    /// Purpose: Stores one backend-ready bind group layout entry.
    struct OpenGlBindGroupLayoutEntry
    {
        uint32 slot = 0U;
        OpenGlBindEntryType type = OpenGlBindEntryType::UNIFORM_BUFFER;
    };
}
