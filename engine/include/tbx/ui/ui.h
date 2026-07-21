#pragma once
#include "tbx/utils/api.h"
#include "tbx/gfx/render_target.h"
#include "tbx/ui/ui_document.h"
#include <string>

// The concrete UI boundary (see cmake/tbx_backend.cmake): ui/rmlui/ implements it and its
// library types never escape that folder. Immediate-mode surface: draw(document) each frame
// you want it on screen — visibility follows what you draw (the render graph's ui pass draws
// enabled Ui blocks; a disabled toy simply is not drawn). Dynamic values flow through the
// generic binding system: set_binding("kills", "3") fills every element carrying
// data-text="kills" (inner text) or data-style="kills" (style attribute).
namespace tbx::ui
{
    /// @brief
    /// Purpose: Draws a document this frame on the screen's UI layer; render() flushes.
    /// Documents are cached by content behind the boundary — calling every frame is the API.
    TBX_API void draw(const UiDocument& document);

    /// @brief
    /// Purpose: Draws a document into an offscreen target immediately (world-space panels,
    /// portraits); the target clears to transparent first.
    TBX_API void draw(const UiDocument& document, const gpu::RenderTarget& target);

    /// @brief
    /// Purpose: Renders everything drawn since the last render() on top of the frame —
    /// called by the render graph's ui pass.
    TBX_API void render();

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
    /// Purpose: Advances animations/layout; called by tbx::run() every frame.
    TBX_API void update(float delta_time);
}
