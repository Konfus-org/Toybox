#pragma once
#include "tbx/math/vectors.h"
#include "tbx/tbx_api.h"

namespace tbx
{
    inline const float PI = 3.14159265358979323846264338327950288f;

    /// <summary>
    /// Purpose: Converts degrees to radians.
    /// Ownership: Returns a value type.
    /// Thread Safety: Stateless; safe to call concurrently.
    /// </summary>
    TBX_API float to_radians(float degrees);

    /// <summary>
    /// Purpose: Converts radians to degrees.
    /// Ownership: Returns a value type.
    /// Thread Safety: Stateless; safe to call concurrently.
    /// </summary>
    TBX_API float to_degrees(float radians);

    /// <summary>
    /// Purpose: Converts degrees to radians.
    /// Ownership: Returns a value type.
    /// Thread Safety: Stateless; safe to call concurrently.
    /// </summary>
    TBX_API Vec3 to_radians(const Vec3& degrees);

    /// <summary>
    /// Purpose: Converts radians to degrees.
    /// Ownership: Returns a value type.
    /// Thread Safety: Stateless; safe to call concurrently.
    /// </summary>
    TBX_API Vec3 to_degrees(const Vec3& radians);

    TBX_API float cos(float x);
    TBX_API float sin(float x);
    TBX_API float tan(float x);
    TBX_API float acos(float x);
    TBX_API float asin(float x);
    TBX_API float atan(float x);
}
