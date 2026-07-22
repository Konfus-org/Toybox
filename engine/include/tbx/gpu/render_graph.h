#pragma once
#include "tbx/api.h"
#include "tbx/assets/assets.h"
#include "tbx/debug/debugging.h"
#include "tbx/ecs/sandbox.h"
#include "tbx/events/events.h"
#include "tbx/gpu/state.h"
#include "tbx/platform/window.h"
#include "tbx/ui/ui.h"
#include "tbx/utils/uuid.h"
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace tbx
{
    /// @brief
    /// Purpose: Everything one frame of rendering reads and writes — the renderer's own
    /// state plus the world, the states the builtin passes resolve through, and the window
    /// being drawn. Built by run() (or a custom host) once per window per frame; passes
    /// never see more of the runtime than this. is_main marks the first window — the one
    /// carrying the shadow map, post chain, and UI.
    struct TBX_API RenderContext
    {
        GpuState& renderer;
        Sandbox& sandbox;
        AssetsState& assets;
        EventsState& events;
        UiState& ui;
        const DebuggingState& debug;
        const Window& window;
        bool is_main = true;
    };

    /// @brief
    /// Purpose: One pass of a frame — a named function drawing its slice through the tbx::gpu
    /// boundary (its own gpu render passes, pipelines, draws).
    struct TBX_API RenderPass
    {
        std::string name = {};
        std::function<void(RenderContext& context)> render = {};
    };

    /// @brief
    /// Purpose: The renderer as plain data — an ordered list of passes run every frame between
    /// gpu_begin_frame and present. make_default_render_graph() builds the standard list
    /// (shadow, geometry with sky, post, ui); games mutate `passes` directly (reorder, erase,
    /// push_back) or build one from scratch. tbx::render() runs it.
    struct TBX_API RenderGraph
    {
        std::vector<RenderPass> passes = {};
    };

    /// @brief
    /// Purpose: The standard pass list: shadow, geometry (with sky), post, ui. run() seeds the
    /// runtime's render_graph with this at boot; games start from it or replace `passes`.
    TBX_API RenderGraph make_default_render_graph();

    /// @brief
    /// Purpose: Runs a render graph for one window — owns the per-window render concerns: binds
    /// the window (make_current) and its viewport, opens the frame (gpu_begin_frame), then runs
    /// every pass in order. run() calls this for each open window; custom hosts call it too.
    TBX_API void render(const RenderGraph& graph, RenderContext& context);

    // The builtin passes — compose custom graphs from them or mix in your own.

    /// @brief
    /// Purpose: Depth from the first directional light into the shadow map.
    TBX_API RenderPass gpu_make_shadow_pass();

    /// @brief
    /// Purpose: Sky + every Renderer toy, lit and shadowed, from the first Camera. When a
    /// PostProcessing block is live the scene lands in an offscreen target for the post pass.
    TBX_API RenderPass gpu_make_geometry_pass();

    /// @brief
    /// Purpose: Runs the live PostProcessing block's shader chain onto the swapchain.
    TBX_API RenderPass gpu_make_post_pass();

    /// @brief
    /// Purpose: Shows every enabled Ui block's document and renders the UI on top.
    TBX_API RenderPass gpu_make_ui_pass();

    /// @brief
    /// Purpose: Drops every render-side cache built from an asset (GPU meshes/textures,
    /// compiled material pipelines, shown UI documents) — wired to the asset system's
    /// unload/reload events so caches follow asset lifetime instead of managing their own.
    TBX_API void gpu_forget_asset(GpuState& renderer, const Uuid& asset_id);
}
