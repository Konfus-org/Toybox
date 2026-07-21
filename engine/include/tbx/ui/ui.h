#pragma once
#include "tbx/utils/api.h"
#include "tbx/gfx/render_target.h"
#include "tbx/ui/ui_document.h"
#include <format>
#include <functional>
#include <string>
#include <string_view>

// The concrete UI boundary (see cmake/tbx_backend.cmake): ui/rmlui/ implements it and its
// library types never escape that folder. Pass-composable shape: draw(document) queues a
// document, draw_to(target) renders everything queued into that texture — and the render
// pass then does whatever it wants with the texture (the builtin ui pass composites it
// fullscreen with the engine ui shaders). update() advances animations and retires
// documents that stopped being drawn. Dynamic values flow through the binding system:
// bind()/set_binding feed elements carrying data-text / data-style attributes.
namespace tbx::ui
{
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
    /// Purpose: Tears the UI down; the next call starts fresh. run() calls this at shutdown.
    TBX_API void reset();

    /// @brief
    /// Purpose: Binds a live data source: every frame the UI reads owner.*field into the
    /// named binding — tbx::ui::bind("kills", state, &GameState::kills) — and documents pick
    /// it up through data-text / data-style. The owner must outlive the binding (unbind()
    /// releases it).
    template <typename TOwner, typename TField>
    void bind(const std::string& name, const TOwner& owner, TField TOwner::* field)
    {
        set_source(name, [&owner, field] { return std::format("{}", owner.*field); });
    }

    /// @brief
    /// Purpose: The engine of bind(): a callable evaluated every update() to feed the named
    /// binding.
    TBX_API void set_source(const std::string& name, std::function<std::string()> source);

    /// @brief
    /// Purpose: Releases a live data source registered by bind()/set_source().
    TBX_API void unbind(const std::string& name);

    /// @brief
    /// Purpose: Sets a named binding value once; drawn documents pick it up through
    /// data-text / data-style attributes (push-style counterpart to bind()).
    TBX_API void set_binding(const std::string& name, const std::string& value);

    /// @brief
    /// Purpose: Numeric convenience for set_binding.
    TBX_API void set_binding(const std::string& name, double value);

    /// @brief
    /// Purpose: Advances animations/layout and retires long-undrawn documents; called by
    /// tbx::run() every frame.
    TBX_API void update(float delta_time);
}
