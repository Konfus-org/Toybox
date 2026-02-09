#pragma once
#include "tbx/graphics/color.h"
#include "tbx/math/vectors.h"
#include "tbx/tbx_api.h"

namespace tbx
{
    /// <summary>
    /// Purpose: Enumerates the supported realtime light source modes.
    /// </summary>
    /// <remarks>
    /// Ownership: Does not own resources.
    /// Thread Safety: Safe to read concurrently.
    /// </remarks>
    enum class LightMode
    {
        Point = 0,
        Area = 1,
        Spot = 2,
        Directional = 3
    };

    /// <summary>
    /// Purpose: Describes a realtime light source for scene rendering.
    /// </summary>
    /// <remarks>
    /// Ownership: Value type; callers own copies and manage component storage.
    /// Thread Safety: Safe to copy between threads; synchronize mutation externally.
    /// </remarks>
    struct TBX_API Light
    {
        /// <summary>
        /// Purpose: Initializes a light with default realtime lighting settings.
        /// </summary>
        /// <remarks>
        /// Ownership: Value type; callers own constructed instances.
        /// Thread Safety: Safe to construct on any thread.
        /// </remarks>
        Light();

        /// <summary>
        /// Purpose: Selects the lighting model used by this light.
        /// </summary>
        /// <remarks>
        /// Ownership: Stored by value.
        /// Thread Safety: Safe to read concurrently; synchronize mutation externally.
        /// </remarks>
        LightMode mode = LightMode::Point;

        /// <summary>
        /// Purpose: Sets the light's base color.
        /// </summary>
        /// <remarks>
        /// Ownership: Stored by value.
        /// Thread Safety: Safe to read concurrently; synchronize mutation externally.
        /// </remarks>
        RgbaColor color = RgbaColor(1.0f, 1.0f, 1.0f, 1.0f);

        /// <summary>
        /// Purpose: Scales the light's brightness contribution.
        /// </summary>
        /// <remarks>
        /// Ownership: Stored by value.
        /// Thread Safety: Safe to read concurrently; synchronize mutation externally.
        /// </remarks>
        float intensity = 1.0f;

        /// <summary>
        /// Purpose: Sets the effective range in world units for point, spot, and area lights.
        /// </summary>
        /// <remarks>
        /// Ownership: Stored by value.
        /// Thread Safety: Safe to read concurrently; synchronize mutation externally.
        /// </remarks>
        float range = 10.0f;

        /// <summary>
        /// Purpose: Sets the inner spotlight angle in degrees.
        /// </summary>
        /// <remarks>
        /// Ownership: Stored by value.
        /// Thread Safety: Safe to read concurrently; synchronize mutation externally.
        /// </remarks>
        float inner_angle = 20.0f;

        /// <summary>
        /// Purpose: Sets the outer spotlight angle in degrees.
        /// </summary>
        /// <remarks>
        /// Ownership: Stored by value.
        /// Thread Safety: Safe to read concurrently; synchronize mutation externally.
        /// </remarks>
        float outer_angle = 35.0f;

        /// <summary>
        /// Purpose: Sets the rectangular area size in world units for area lights.
        /// </summary>
        /// <remarks>
        /// Ownership: Stored by value.
        /// Thread Safety: Safe to read concurrently; synchronize mutation externally.
        /// </remarks>
        Vec2 area_size = Vec2(1.0f, 1.0f);
    };
}
