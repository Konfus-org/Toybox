#pragma once
#include "tbx/tbx_api.h"
#include <format>
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

}

template <>
struct std::formatter<tbx::Bounds>
{
    constexpr auto parse(std::format_parse_context& ctx)
    {
        return _formatter.parse(ctx);
    }

    template <typename TFormatContext>
    auto format(const tbx::Bounds& bounds, TFormatContext& ctx) const
    {
        return _formatter.format(
            std::format(
                "[Left: {}, Right: {}, Top: {}, Bottom: {}]",
                bounds.left,
                bounds.right,
                bounds.top,
                bounds.bottom),
            ctx);
    }

    std::formatter<std::string> _formatter;
};
