#pragma once
#include "tbx/gfx/render_target.h"
#include "tbx/math/math.h"
#include "tbx/ui/ui_block.h"
#include "tbx/ui/ui_document.h"
#include "tbx/utils/api.h"
#include "tbx/utils/color.h"
#include <format>
#include <functional>
#include <string>
#include <string_view>
#include <utility>

// The concrete UI boundary (see cmake/tbx_backend.cmake): ui/rmlui/ implements it and its
// library types never escape that folder. No queues, no implicit targets: draw(document,
// target) rasters that document into that texture right now, and the render pass owns what
// happens to the texture afterwards (the builtin ui pass composites each Ui block's texture
// through its own gpu pipeline). update() advances animations and retires documents that
// stopped being drawn.
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
    /// Purpose: Rasters one document into one target right now (cleared to transparent,
    /// premultiplied alpha). Documents are cached by content behind the boundary — drawing
    /// every frame is the API; what is not drawn disappears. Shading is not the document's
    /// business: passes set gpu pipelines around the textures this produces.
    TBX_API void draw(const UiDocument& document, const gpu::RenderTarget& target);

    /// @brief
    /// Purpose: Tears the UI down; the next call starts fresh. run() calls this at shutdown.
    TBX_API void reset();

    /// @brief
    /// Purpose: Advances animations/layout, evaluates bindings, and retires long-undrawn
    /// documents; called by tbx::run() every frame.
    TBX_API void update(float delta_time);

    /// @brief
    /// Purpose: Registers a binding (replacing any with the same name); its source runs
    /// every update().
    TBX_API void bind(UiBinding binding);

    // The live bind family: this property binds to that UI element — mutate the variable
    // and the element follows (the variable must outlive the binding; unbind() releases).
    // Elements consume values via data-text (inner text), data-width/data-height (bar sizes,
    // scaled by data-width-scale/data-height-scale), or data-style (raw style, engine use).

    /// @brief
    /// Purpose: Binds a bool to the named element slot.
    inline void bind(const bool& value, std::string name)
    {
        auto source = [&value]
        {
            return std::string(value ? "true" : "false");
        };
        bind({.name = std::move(name), .source = std::move(source)});
    }

    /// @brief
    /// Purpose: Binds a color (#rrggbbaa) to the named element slot.
    inline void bind(const Color& value, std::string name)
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
    /// Purpose: Binds a float to the named element slot.
    inline void bind(const float& value, std::string name)
    {
        auto source = [&value]
        {
            return std::format("{}", value);
        };
        bind({.name = std::move(name), .source = std::move(source)});
    }

    /// @brief
    /// Purpose: Binds an int to the named element slot.
    inline void bind(const int& value, std::string name)
    {
        auto source = [&value]
        {
            return std::format("{}", value);
        };
        bind({.name = std::move(name), .source = std::move(source)});
    }

    /// @brief
    /// Purpose: Binds a string to the named element slot.
    inline void bind(const std::string& value, std::string name)
    {
        auto source = [&value]
        {
            return value;
        };
        bind({.name = std::move(name), .source = std::move(source)});
    }

    /// @brief
    /// Purpose: Binds a Vec2 ("x, y") to the named element slot.
    inline void bind(const Vec2& value, std::string name)
    {
        auto source = [&value]
        {
            return std::format("{}, {}", value.x, value.y);
        };
        bind({.name = std::move(name), .source = std::move(source)});
    }

    /// @brief
    /// Purpose: Binds a Vec3 ("x, y, z") to the named element slot.
    inline void bind(const Vec3& value, std::string name)
    {
        auto source = [&value]
        {
            return std::format("{}, {}, {}", value.x, value.y, value.z);
        };
        bind({.name = std::move(name), .source = std::move(source)});
    }

    /// @brief
    /// Purpose: Releases the binding with the given name.
    TBX_API void unbind(const std::string& name);

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
}
