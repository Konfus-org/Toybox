#pragma once
#include "tbx/tbx_api.h"
#include <string>
namespace tbx
{
    /// <summary>
    /// Represents axis-aligned bounds for projection calculations.
    /// Ownership: value type; callers own copies and may store or return them freely.
    /// Thread Safety: immutable value semantics; safe for concurrent use when not shared mutably.
    /// </summary>
    struct TBX_API Bounds
    {
      public:
        Bounds() = default;

        /// <summary>
        /// Initializes the bounds with explicit edges.
        /// Ownership: stores the provided values by copy.
        /// Thread Safety: safe for concurrent use when not sharing mutable instances.
        /// </summary>
        Bounds(float left, float right, float top, float bottom) : Left(left), Right(right), Top(top), Bottom(bottom) {}

        /// <summary>
        /// Builds bounds for an orthographic projection using the given size and aspect ratio.
        /// Ownership: returns a bounds instance by value; the caller owns the copy.
        /// Thread Safety: stateless; safe to call concurrently.
        /// </summary>
        static Bounds FromOrthographicProjection(float size, float aspect);

        /// <summary>
        /// Builds bounds for a perspective projection using the provided FOV, aspect ratio, and near plane.
        /// Ownership: returns a bounds instance by value; the caller owns the copy.
        /// Thread Safety: stateless; safe to call concurrently.
        /// </summary>
        static Bounds FromPerspectiveProjection(float fov, float aspectRatio, float zNear);

        static Bounds Identity;

        float Left = 0.0f;
        float Right = 0.0f;
        float Top = 0.0f;
        float Bottom = 0.0f;
    };

    /// <summary>
    /// Converts bounds into a descriptive string for debugging.
    /// Ownership: returns a string by value; the caller owns the result.
    /// Thread Safety: stateless; safe to call concurrently.
    /// </summary>
    std::string to_string(const Bounds& bounds);
}
