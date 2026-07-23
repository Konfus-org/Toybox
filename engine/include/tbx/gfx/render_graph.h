#pragma once
#include "tbx/api.h"
#include "tbx/utils/uuid.h"
#include <functional>
#include <string>
#include <vector>

namespace tbx
{
    // RenderContext holds these only by reference, so forward declarations suffice — and,
    // crucially, keeping state.h out of this header lets RenderState (the render state) own the
    // RenderGraph without an include cycle. The .cpp files that dereference these members
    // include the full headers themselves.
    struct RenderState;
    class Sandbox;
    struct AssetsState;
    struct EventsState;
    struct UiState;
    struct DebuggingState;
    struct Window;

    /// @brief
    /// Purpose: Everything one frame of rendering reads and writes — the renderer's own
    /// state plus the world, the states the builtin passes resolve through, and the window
    /// being drawn. Built by run() (or a custom host) once per window per frame; passes
    /// never see more of the runtime than this. is_main marks the first window — the one
    /// carrying the shadow map, post chain, and UI.
    ///
    /// Custom passes: push a RenderPass into renderer.render_graph.passes and draw through the
    /// gpu boundary. The frame's camera + light data (view_projection, camera_position, light_*)
    /// lives on renderer.frame, and the builtin shaders/meshes/targets on renderer — so place a
    /// scene-reading pass AFTER make_geometry_shadow_pass(), which populates renderer.frame.
    struct TBX_DLL_EXPORT RenderContext
    {
        RenderState& renderer;
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
    struct TBX_DLL_EXPORT RenderPass
    {
        std::string name = {};
        std::function<void(RenderContext& context)> render = {};
    };

    /// @brief
    /// Purpose: The renderer as plain data — an ordered list of passes run every frame between
    /// begin_render_frame and present. make_default_render_graph() builds the standard list
    /// (shadow, geometry with sky, post, ui); games mutate `passes` directly (reorder, erase,
    /// push_back) or build one from scratch. tbx::render() runs it.
    struct TBX_DLL_EXPORT RenderGraph
    {
        std::vector<RenderPass> passes = {};
    };

    /// @brief
    /// Purpose: The standard pass list: shadow, geometry (with sky), post, ui. run() seeds the
    /// runtime's render_graph with this at boot; games start from it or replace `passes`.
    TBX_DLL_EXPORT RenderGraph make_default_render_graph();

    /// @brief
    /// Purpose: Runs the render state's graph (context.renderer.render_graph) for one window —
    /// owns the per-window render concerns: binds the window (make_current) and its viewport,
    /// opens the frame (begin_render_frame), then runs every pass in order. run() calls this for
    /// each open window; custom hosts set renderer.render_graph.passes and call it too.
    TBX_DLL_EXPORT void render(RenderContext& context);

    // The builtin passes — compose custom graphs from them or mix in your own.

    /// @brief
    /// Purpose: Depth from the first directional light into the shadow map.
    TBX_DLL_EXPORT RenderPass make_shadow_render_pass();

    /// @brief
    /// Purpose: Sky + every Renderer toy, lit and shadowed, from the first Camera. When a
    /// PostProcessing block is live the scene lands in an offscreen target for the post pass.
    TBX_DLL_EXPORT RenderPass make_geometry_shadow_pass();

    /// @brief
    /// Purpose: Runs the live PostProcessing block's shader chain onto the swapchain.
    TBX_DLL_EXPORT RenderPass make_post_render_pass();

    /// @brief
    /// Purpose: Shows every enabled Ui block's document and renders the UI on top.
    TBX_DLL_EXPORT RenderPass make_ui_render_pass();

    /// @brief
    /// Purpose: Drops every render-side cache built from an asset (GPU meshes/textures,
    /// compiled material pipelines, shown UI documents) — wired to the asset system's
    /// unload/reload events so caches follow asset lifetime instead of managing their own.
    TBX_DLL_EXPORT void gpu_purge(RenderState& renderer, const Uuid& asset_id);
}
