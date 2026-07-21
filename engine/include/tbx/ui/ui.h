#pragma once
#include "tbx/utils/api.h"
#include "tbx/gfx/render_target.h"
#include "tbx/math/math.h"
#include "tbx/ui/ui_document.h"
#include "tbx/utils/color.h"
#include <format>
#include <functional>
#include <string>
#include <string_view>
#include <utility>

// The concrete UI boundary (see cmake/tbx_backend.cmake): ui/rmlui/ implements it and its
// library types never escape that folder. Pass-composable shape: draw(document) queues a
// document, draw_to(target) renders everything queued into that texture — and the render
// pass then does whatever it wants with the texture (the builtin ui pass composites it
// fullscreen with the engine ui shaders). update() advances animations and retires
// documents that stopped being drawn.
//
// Dynamic values flow through UiBinding objects: a binding links a document slot (elements
// carrying data-text="name" / data-style="name") to a value source. bind_to() links a live
// variable, bind() takes any hand-built binding, and the typed set_* family pushes one-off
// values.
namespace tbx::ui
{
    /// @brief
    /// Purpose: THE link between two things: a named document slot (data-text / data-style
    /// attributes) and the source producing its value, evaluated every update().
    struct TBX_API UiBinding
    {
        std::string name = {};
        std::function<std::string()> source = {};
    };

    /// @brief
    /// Purpose: Queues a document for the next draw_to(), optionally shaded by custom
    /// vertex/fragment stages (empty = the builtin ui shaders under resources/Shaders/Tbx).
    /// Documents are cached by content behind the boundary — drawing every frame is the API;
    /// what is not drawn disappears.
    TBX_API void draw(
        const UiDocument& document,
        std::string_view vertex_shader = {},
        std::string_view fragment_shader = {});

    /// @brief
    /// Purpose: Renders everything queued by draw() into the target (cleared to transparent,
    /// premultiplied alpha) and empties the queue — the pass owns what happens to the
    /// texture afterwards.
    TBX_API void draw_to(const gpu::RenderTarget& target);

    /// @brief
    /// Purpose: Registers a binding (replacing any with the same name); its source runs
    /// every update().
    TBX_API void bind(UiBinding binding);

    /// @brief
    /// Purpose: Releases the binding with the given name.
    TBX_API void unbind(const std::string& name);

    /// @brief
    /// Purpose: Tears the UI down; the next call starts fresh. run() calls this at shutdown.
    TBX_API void reset();

    /// @brief
    /// Purpose: Advances animations/layout, evaluates bindings, and retires long-undrawn
    /// documents; called by tbx::run() every frame.
    TBX_API void update(float delta_time);

    // The typed one-off setters: push a value into a named slot right now.

    /// @brief
    /// Purpose: Sets a slot to a string value.
    TBX_API void set_string(const std::string& name, std::string value);

    /// @brief
    /// Purpose: Sets a slot to "true"/"false".
    TBX_API void set_bool(const std::string& name, bool value);

    /// @brief
    /// Purpose: Sets a slot to a color as #rrggbbaa (drops straight into styles).
    TBX_API void set_color(const std::string& name, const Color& value);

    /// @brief
    /// Purpose: Sets a slot to a float (trailing zeros trimmed).
    TBX_API void set_float(const std::string& name, float value);

    /// @brief
    /// Purpose: Sets a slot to an integer.
    TBX_API void set_int(const std::string& name, int value);

    /// @brief
    /// Purpose: Sets a slot to "x, y".
    TBX_API void set_vec2(const std::string& name, const Vec2& value);

    /// @brief
    /// Purpose: Sets a slot to "x, y, z".
    TBX_API void set_vec3(const std::string& name, const Vec3& value);

    // The live bind_to family: the slot follows the referenced variable (which must outlive
    // the binding; unbind() releases it). Each returns through bind() with a UiBinding.

    /// @brief
    /// Purpose: Live-links a slot to a bool.
    inline void bind_to(std::string name, const bool& value)
    {
        auto source = [&value] { return std::string(value ? "true" : "false"); };
        bind({.name = std::move(name), .source = std::move(source)});
    }

    /// @brief
    /// Purpose: Live-links a slot to a color (#rrggbbaa).
    inline void bind_to(std::string name, const Color& value)
    {
        auto source = [&value]
        {
            return std::format(
                "#{:02x}{:02x}{:02x}{:02x}",
                static_cast<int>(value.r * 255.0f),
                static_cast<int>(value.g * 255.0f),
                static_cast<int>(value.b * 255.0f),
                static_cast<int>(value.a * 255.0f));
        };
        bind({.name = std::move(name), .source = std::move(source)});
    }

    /// @brief
    /// Purpose: Live-links a slot to a float.
    inline void bind_to(std::string name, const float& value)
    {
        auto source = [&value] { return std::format("{}", value); };
        bind({.name = std::move(name), .source = std::move(source)});
    }

    /// @brief
    /// Purpose: Live-links a slot to an int.
    inline void bind_to(std::string name, const int& value)
    {
        auto source = [&value] { return std::format("{}", value); };
        bind({.name = std::move(name), .source = std::move(source)});
    }

    /// @brief
    /// Purpose: Live-links a slot to a string.
    inline void bind_to(std::string name, const std::string& value)
    {
        auto source = [&value] { return value; };
        bind({.name = std::move(name), .source = std::move(source)});
    }

    /// @brief
    /// Purpose: Live-links a slot to a Vec2 ("x, y").
    inline void bind_to(std::string name, const Vec2& value)
    {
        auto source = [&value] { return std::format("{}, {}", value.x, value.y); };
        bind({.name = std::move(name), .source = std::move(source)});
    }

    /// @brief
    /// Purpose: Live-links a slot to a Vec3 ("x, y, z").
    inline void bind_to(std::string name, const Vec3& value)
    {
        auto source = [&value]
        { return std::format("{}, {}, {}", value.x, value.y, value.z); };
        bind({.name = std::move(name), .source = std::move(source)});
    }
}
