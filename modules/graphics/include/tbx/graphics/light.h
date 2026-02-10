#pragma once
#include "tbx/graphics/color.h"
#include "tbx/math/vectors.h"
#include "tbx/tbx_api.h"

namespace tbx
{
    /// <summary>
    /// Purpose: Defines common light properties shared by all realtime light types.
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
        /// Notes: The renderer normalizes the light color so intensity scales total light energy
        /// independent of hue (e.g., a red light at intensity 1.0 should be comparable to a
        /// white light at intensity 1.0).
        /// </remarks>
        float intensity = 1.0f;
    };

    /// <summary>
    /// Purpose: Describes a point light source for scene rendering.
    /// </summary>
    /// <remarks>
    /// Ownership: Value type; callers own copies and manage component storage.
    /// Thread Safety: Safe to copy between threads; synchronize mutation externally.
    /// </remarks>
    struct TBX_API PointLight : public Light
    {
        /// <summary>
        /// Purpose: Initializes a point light with default realtime lighting settings.
        /// </summary>
        /// <remarks>
        /// Ownership: Value type; callers own constructed instances.
        /// Thread Safety: Safe to construct on any thread.
        /// </remarks>
        PointLight();

        /// <summary>
        /// Purpose: Sets the effective range in world units for point lights.
        /// </summary>
        /// <remarks>
        /// Ownership: Stored by value.
        /// Thread Safety: Safe to read concurrently; synchronize mutation externally.
        /// </remarks>
        float range = 10.0f;
    };

    /// <summary>
    /// Purpose: Describes a spot light source for scene rendering.
    /// </summary>
    /// <remarks>
    /// Ownership: Value type; callers own copies and manage component storage.
    /// Thread Safety: Safe to copy between threads; synchronize mutation externally.
    /// </remarks>
    struct TBX_API SpotLight : public Light
    {
        /// <summary>
        /// Purpose: Initializes a spot light with default realtime lighting settings.
        /// </summary>
        /// <remarks>
        /// Ownership: Value type; callers own constructed instances.
        /// Thread Safety: Safe to construct on any thread.
        /// </remarks>
        SpotLight();

        /// <summary>
        /// Purpose: Sets the effective range in world units for spot lights.
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
    };

    /// <summary>
    /// Purpose: Describes an area light source for scene rendering.
    /// </summary>
    /// <remarks>
    /// Ownership: Value type; callers own copies and manage component storage.
    /// Thread Safety: Safe to copy between threads; synchronize mutation externally.
    /// </remarks>
    struct TBX_API AreaLight : public Light
    {
        /// <summary>
        /// Purpose: Initializes an area light with default realtime lighting settings.
        /// </summary>
        /// <remarks>
        /// Ownership: Value type; callers own constructed instances.
        /// Thread Safety: Safe to construct on any thread.
        /// </remarks>
        AreaLight();

        /// <summary>
        /// Purpose: Sets the effective range in world units for area lights.
        /// </summary>
        /// <remarks>
        /// Ownership: Stored by value.
        /// Thread Safety: Safe to read concurrently; synchronize mutation externally.
        /// </remarks>
        float range = 10.0f;

        /// <summary>
        /// Purpose: Sets the rectangular area size in world units for area lights.
        /// </summary>
        /// <remarks>
        /// Ownership: Stored by value.
        /// Thread Safety: Safe to read concurrently; synchronize mutation externally.
        /// </remarks>
        Vec2 area_size = Vec2(1.0f, 1.0f);
    };

    /// <summary>
    /// Purpose: Describes a directional light source for scene rendering.
    /// </summary>
    /// <remarks>
    /// Ownership: Value type; callers own copies and manage component storage.
    /// Thread Safety: Safe to copy between threads; synchronize mutation externally.
    /// </remarks>
    struct TBX_API DirectionalLight : public Light
    {
        /// <summary>
        /// Purpose: Initializes a directional light with default realtime lighting settings.
        /// </summary>
        /// <remarks>
        /// Ownership: Value type; callers own constructed instances.
        /// Thread Safety: Safe to construct on any thread.
        /// </remarks>
        DirectionalLight();

        /// <summary>
        /// Purpose: Sets the ambient lighting contribution produced by this directional light.
        /// </summary>
        /// <remarks>
        /// Ownership: Stored by value.
        /// Thread Safety: Safe to read concurrently; synchronize mutation externally.
        /// Notes: The renderer tints this ambient term using the directional light color and sums
        /// contributions across all directional lights.
        /// </remarks>
        float ambient = 0.03f;
    };
}

