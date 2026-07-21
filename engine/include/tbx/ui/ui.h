#pragma once
#include "tbx/utils/api.h"
#include "tbx/gfx/render_target.h"
#include "tbx/ui/ui_document.h"
#include <string>

// The concrete UI boundary (see cmake/tbx_backend.cmake): ui/rmlui/ implements it and its
// library types never escape that folder. draw(document) renders a document right now —
// render passes call it however they want (the builtin ui pass draws every enabled Ui
// block); update() advances animations and retires documents that stopped being drawn.
// Dynamic values flow through the generic binding system: set_binding("kills", "3") fills
// every element carrying data-text="kills" (inner text) or data-style="kills" (style).
namespace tbx::ui
{
    /// @brief
    /// Purpose: Draws a document into the current frame right now. Documents are cached by
    /// content behind the boundary — drawing every frame is the API; what is not drawn
    /// disappears.
    TBX_API void draw(const UiDocument& document);

    /// @brief
    /// Purpose: Draws a document into an offscreen target (world-space panels, portraits);
    /// the target clears to transparent first.
    TBX_API void draw(const UiDocument& document, const gpu::RenderTarget& target);

    /// @brief
    /// Purpose: Tears the UI down; the next call starts fresh. run() calls this at shutdown.
    TBX_API void reset();

    /// @brief
    /// Purpose: Sets a named binding value; drawn documents pick it up through data-text /
    /// data-style attributes.
    TBX_API void set_binding(const std::string& name, const std::string& value);

    /// @brief
    /// Purpose: Numeric convenience for set_binding.
    TBX_API void set_binding(const std::string& name, double value);

    /// @brief
    /// Purpose: Advances animations/layout and retires long-undrawn documents; called by
    /// tbx::run() every frame.
    TBX_API void update(float delta_time);
}
