#pragma once
#include "tbx/graphics/color.h"
#include "tbx/math/vectors.h"
#include "tbx/tbx_api.h"

namespace tbx
{
    /// @brief
    /// Purpose: Defines common light properties shared by all realtime light types.
    /// @details
    /// Ownership: Value type; callers own copies and manage component storage.
    /// Thread Safety: Safe to copy between threads; synchronize mutation externally.

    struct TBX_API Light
    {
        Light();

        /// @brief
        /// Purpose: Sets the light's base color.
        /// @details
        /// Ownership: Stored by value.
        /// Thread Safety: Safe to read concurrently; synchronize mutation externally.

        Color color = Color(1.0f, 1.0f, 1.0f, 1.0f);

        /// @brief
        /// Purpose: Scales the light's brightness contribution.
        /// @details
        /// Ownership: Stored by value.
        /// Thread Safety: Safe to read concurrently; synchronize mutation externally. Notes: The
        /// renderer normalizes the light color so intensity scales total light energy independent
        /// of hue (e.g., a red light at intensity 1.0 should be comparable to a white light at
        /// intensity 1.0).

        float intensity = 1.0f;
    };

    /// @brief
    /// Purpose: Describes a point light source for scene rendering.
    /// @details
    /// Ownership: Value type; callers own copies and manage component storage.
    /// Thread Safety: Safe to copy between threads; synchronize mutation externally.

    struct TBX_API PointLight : public Light
    {
        PointLight();
        PointLight(Color color, float intensity = 1.0f, float range = 10.0f);

        /// @brief
        /// Purpose: Sets the effective range in world units for point lights.
        /// @details
        /// Ownership: Stored by value.
        /// Thread Safety: Safe to read concurrently; synchronize mutation externally.

        float range = 10.0f;

        /// @brief
        /// Purpose: Controls whether this point light renders realtime shadows.
        /// @details
        /// Ownership: Stored by value.
        /// Thread Safety: Safe to read concurrently; synchronize mutation externally.

        bool shadows_enabled = true;
    };

    /// @brief
    /// Purpose: Describes a spot light source for scene rendering.
    /// @details
    /// Ownership: Value type; callers own copies and manage component storage.
    /// Thread Safety: Safe to copy between threads; synchronize mutation externally.

    struct TBX_API SpotLight : public Light
    {
        SpotLight();
        SpotLight(
            Color color,
            float intensity = 1.0f,
            float range = 10.0f,
            float inner_angle = 20.0f,
            float outer_angle = 35.0f);

        /// @brief
        /// Purpose: Sets the effective range in world units for spot lights.
        /// @details
        /// Ownership: Stored by value.
        /// Thread Safety: Safe to read concurrently; synchronize mutation externally.

        float range = 10.0f;

        /// @brief
        /// Purpose: Sets the inner spotlight angle in degrees.
        /// @details
        /// Ownership: Stored by value.
        /// Thread Safety: Safe to read concurrently; synchronize mutation externally.

        float inner_angle = 20.0f;

        /// @brief
        /// Purpose: Sets the outer spotlight angle in degrees.
        /// @details
        /// Ownership: Stored by value.
        /// Thread Safety: Safe to read concurrently; synchronize mutation externally.

        float outer_angle = 35.0f;
    };

    /// @brief
    /// Purpose: Describes an area light source for scene rendering.
    /// @details
    /// Ownership: Value type; callers own copies and manage component storage.
    /// Thread Safety: Safe to copy between threads; synchronize mutation externally.

    struct TBX_API AreaLight : public Light
    {
        AreaLight();
        AreaLight(
            Color color,
            float intensity = 1.0f,
            float range = 10.0f,
            Vec2 area_size = Vec2(1.0f, 1.0f));

        /// @brief
        /// Purpose: Sets the effective range in world units for area lights.
        /// @details
        /// Ownership: Stored by value.
        /// Thread Safety: Safe to read concurrently; synchronize mutation externally.

        float range = 10.0f;

        /// @brief
        /// Purpose: Sets the rectangular area size in world units for area lights.
        /// @details
        /// Ownership: Stored by value.
        /// Thread Safety: Safe to read concurrently; synchronize mutation externally.

        Vec2 area_size = Vec2(1.0f, 1.0f);
    };

    /// @brief
    /// Purpose: Describes a directional light source for scene rendering.
    /// @details
    /// Ownership: Value type; callers own copies and manage component storage.
    /// Thread Safety: Safe to copy between threads; synchronize mutation externally.

    struct TBX_API DirectionalLight : public Light
    {
        DirectionalLight();
        DirectionalLight(Color color, float intensity = 1.0f, float ambient = 0.03f);

        /// @brief
        /// Purpose: Sets the ambient lighting contribution produced by this directional light.
        /// @details
        /// Ownership: Stored by value.
        /// Thread Safety: Safe to read concurrently; synchronize mutation externally. Notes: The
        /// renderer tints this ambient term using the directional light color and sums
        /// contributions across all directional lights.

        float ambient = 0.03f;
    };
}
