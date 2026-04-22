#pragma once
#include "tbx/systems/math/vectors.h"
#include "tbx/tbx_api.h"

namespace tbx
{
    inline const float PI = 3.14159265358979323846264338327950288f;

    /// @brief
    /// Purpose: Converts degrees to radians.
    /// @details
    /// Ownership: Returns a value type.
    /// Thread Safety: Stateless; safe to call concurrently.
    TBX_API float to_radians(float degrees);

    /// @brief
    /// Purpose: Converts radians to degrees.
    /// @details
    /// Ownership: Returns a value type.
    /// Thread Safety: Stateless; safe to call concurrently.
    TBX_API float to_degrees(float radians);

    /// @brief
    /// Purpose: Converts degrees to radians.
    /// @details
    /// Ownership: Returns a value type.
    /// Thread Safety: Stateless; safe to call concurrently.
    TBX_API Vec3 to_radians(const Vec3& degrees);

    /// @brief
    /// Purpose: Converts radians to degrees.
    /// @details
    /// Ownership: Returns a value type.
    /// Thread Safety: Stateless; safe to call concurrently.
    TBX_API Vec3 to_degrees(const Vec3& radians);

    /// @brief
    /// Purpose: Returns the smaller of two scalar values.
    /// @details
    /// Ownership: Returns a value type.
    /// Thread Safety: Stateless; safe to call concurrently.
    TBX_API float min(float left, float right);

    /// @brief
    /// Purpose: Returns the larger of two scalar values.
    /// @details
    /// Ownership: Returns a value type.
    /// Thread Safety: Stateless; safe to call concurrently.
    TBX_API float max(float left, float right);

    /// @brief
    /// Purpose: Clamps a scalar to the inclusive range [minimum_value, maximum_value].
    /// @details
    /// Ownership: Returns a value type.
    /// Thread Safety: Stateless; safe to call concurrently.
    TBX_API float clamp(float value, float minimum_value, float maximum_value);

    /// @brief
    /// Purpose: Computes the square root of a scalar value.
    /// @details
    /// Ownership: Returns a value type.
    /// Thread Safety: Stateless; safe to call concurrently.
    TBX_API float sqrt(float value);

    /// @brief
    /// Purpose: Rounds a scalar down toward negative infinity.
    /// @details
    /// Ownership: Returns a value type.
    /// Thread Safety: Stateless; safe to call concurrently.
    TBX_API float floor(float value);

    /// @brief
    /// Purpose: Rounds a scalar up toward positive infinity.
    /// @details
    /// Ownership: Returns a value type.
    /// Thread Safety: Stateless; safe to call concurrently.
    TBX_API float ceil(float value);

    TBX_API float cos(float x);
    TBX_API float sin(float x);
    TBX_API float tan(float x);
    TBX_API float acos(float x);
    TBX_API float asin(float x);
    TBX_API float atan(float x);
}
