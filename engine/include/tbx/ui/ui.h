#pragma once
#include "tbx/api.h"
#include "tbx/utils/color.h"
#include "tbx/gfx/render_target.h"
#include "tbx/math/math.h"
#include "tbx/ui/document.h"
#include "tbx/ui/font.h"
#include "tbx/ui/ui_block.h"
#include <format>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>

// The concrete UI boundary (see cmake/tbx_backend.cmake): ui/rmlui/ implements it and its
// library types never escape that folder. No queues, no implicit targets: draw_ui(document,
// target) rasters that document into that texture right now, and the render pass owns what
// happens to the texture afterwards (the builtin ui pass composites each Ui block's texture
// through its own gpu pipeline). update_ui() advances animations and retires documents that
// stopped being drawn.
//
// Dynamic values flow through Binding objects: a binding links a document slot (elements
// carrying data-text="name" / data-style="name") to a value source. bind_to() links a live
// variable, bind() takes any hand-built binding, and the typed set_* family pushes one-off
// values.
namespace tbx
{
    /// @brief
    /// Purpose: THE link between two things: a named document slot (data-text / data-style
    /// attributes) and the source producing its value, evaluated every update_ui().
    struct TBX_API Binding
    {
        std::string name = {};
        std::function<std::string()> source = {};
    };

    /// @brief
    /// Purpose: The ui module's state, held by value on the Runtime: the global binding tables
    /// are plain data — push a value with runtime.ui.bindings["slot"] = value (engine slots:
    /// world-anchor labels, debug overlay). Per-document live values live on the owning UI
    /// block instead (UI::bindings). The document/render stack lives behind the backend seam
    /// (ui/rmlui/ defines Backend; library types never escape that folder), built lazily on the
    /// first draw.
    struct TBX_API UiState
    {
        UiState();
        ~UiState();

        UiState(const UiState&) = delete;
        UiState& operator=(const UiState&) = delete;

        UiState(UiState&& other) noexcept;
        UiState& operator=(UiState&& other) noexcept;

        std::unordered_map<std::string, std::string> bindings; // slot -> latest value
        std::unordered_map<std::string, Binding> live_bindings; // evaluated every update
        struct Backend; // defined by the ui backend's .cpp
        std::unique_ptr<Backend> backend;
    };

    /// @brief
    /// Purpose: Rasters one document into one target right now (cleared to transparent,
    /// premultiplied alpha). Documents are cached by content behind the boundary — drawing
    /// every frame is the API; what is not drawn disappears. Shading is not the document's
    /// business: passes set gpu pipelines around the textures this produces. `owner` is the UI
    /// block this document belongs to: its per-block `bindings` are evaluated and resolve a
    /// slot before the global bindings map (nullptr for engine layers like the debug overlay).
    TBX_API void draw_ui(
        UiState& state,
        const Document& document,
        const RenderTarget& target,
        const UI* owner);

    /// @brief
    /// Purpose: Registers a font face under a family name — documents reference it via
    /// font-family in their styles. Faces are fallback-capable; call once per face (the
    /// runtime sets the engine's builtin font at boot, games may add more). The boundary
    /// keeps its own copy of the bytes, so the asset may unload freely.
    TBX_API void set_font(UiState& state, const Font& font, const std::string& family);

    /// @brief
    /// Purpose: Advances animations/layout, evaluates bindings, and retires long-undrawn
    /// documents. Called by tbx::run() every frame.
    TBX_API void update_ui(UiState& state, float delta_time);

    /// @brief
    /// Purpose: Registers a binding (replacing any with the same name); its source runs
    /// every update_ui().
    TBX_API void bind(UiState& state, Binding binding);

    // The live bind family: this property binds to that UI element — mutate the variable
    // and the element follows (the variable must outlive the binding; unbind() releases).
    // Elements consume values via data-text (inner text), data-width/data-height (bar sizes,
    // scaled by data-width-scale/data-height-scale), or data-style (raw style, engine use).

    /// @brief
    /// Purpose: Binds a bool to the named element slot.
    inline void bind(UiState& state, const bool& value, std::string name)
    {
        auto source = [&value]
        {
            return std::string(value ? "true" : "false");
        };
        bind(state, {.name = std::move(name), .source = std::move(source)});
    }

    /// @brief
    /// Purpose: Binds a color (#rrggbbaa) to the named element slot.
    inline void bind(UiState& state, const Color& value, std::string name)
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
        bind(state, {.name = std::move(name), .source = std::move(source)});
    }

    /// @brief
    /// Purpose: Binds a float to the named element slot.
    inline void bind(UiState& state, const float& value, std::string name)
    {
        auto source = [&value]
        {
            return std::format("{}", value);
        };
        bind(state, {.name = std::move(name), .source = std::move(source)});
    }

    /// @brief
    /// Purpose: Binds an int to the named element slot.
    inline void bind(UiState& state, const int& value, std::string name)
    {
        auto source = [&value]
        {
            return std::format("{}", value);
        };
        bind(state, {.name = std::move(name), .source = std::move(source)});
    }

    /// @brief
    /// Purpose: Binds a string to the named element slot.
    inline void bind(UiState& state, const std::string& value, std::string name)
    {
        auto source = [&value]
        {
            return value;
        };
        bind(state, {.name = std::move(name), .source = std::move(source)});
    }

    /// @brief
    /// Purpose: Binds a Vec2 ("x, y") to the named element slot.
    inline void bind(UiState& state, const Vec2& value, std::string name)
    {
        auto source = [&value]
        {
            return std::format("{}, {}", value.x, value.y);
        };
        bind(state, {.name = std::move(name), .source = std::move(source)});
    }

    /// @brief
    /// Purpose: Binds a Vec3 ("x, y, z") to the named element slot.
    inline void bind(UiState& state, const Vec3& value, std::string name)
    {
        auto source = [&value]
        {
            return std::format("{}, {}, {}", value.x, value.y, value.z);
        };
        bind(state, {.name = std::move(name), .source = std::move(source)});
    }

}
