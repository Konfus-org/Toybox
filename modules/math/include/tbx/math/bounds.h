#pragma once
#include "tbx/tbx_api.h"
#include <string>

namespace tbx
{
    // Represents axis-aligned bounds for projection calculations.
    // Ownership: value type; callers own copies and may store or return them freely.
    // Thread Safety: immutable value semantics; safe for concurrent use when not shared mutably.
    struct TBX_API Bounds
    {
      public:
        Bounds();

        // Initializes the bounds with explicit edges.
        // Ownership: stores the provided values by copy.
        // Thread Safety: safe for concurrent use when not sharing mutable instances.
        Bounds(float l, float r, float t, float b);

        // Builds bounds for an orthographic projection using the given size and aspect ratio.
        // Ownership: returns a bounds instance by value; the caller owns the copy.
        // Thread Safety: stateless; safe to call concurrently.
        static Bounds from_orthographic_projection(float size, float aspect);

        // Builds bounds for a perspective projection using the provided FOV, aspect ratio, and
        // near plane. Ownership: returns a bounds instance by value; the caller owns the copy.
        // Thread Safety: stateless; safe to call concurrently.
        static Bounds from_perspective_projection(float fov, float aspect_ratio, float z_near);

        float left = 0.0f;
        float right = 0.0f;
        float top = 0.0f;
        float bottom = 0.0f;
    };

    // Converts bounds into a descriptive string for debugging.
    // Ownership: returns a string by value; the caller owns the result.
    // Thread Safety: stateless; safe to call concurrently.
    TBX_API String to_string(const Bounds& bounds);
}
