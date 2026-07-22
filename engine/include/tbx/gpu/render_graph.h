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
        const DebugViewState& debug;
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
    /// Purpose: The standard renderer: an ordered list of passes run every frame between
    /// gpu_begin_frame and present. The default list renders a sandbox completely (shadow,
    /// geometry with sky, post, ui); games reorder, remove, or append passes — or build a
    /// graph from scratch — to author their own rendering.
    class TBX_API RenderGraph final
    {
      public:
        /// @brief
        /// Purpose: Starts as the standard pass list (shadow, geometry with sky, post, ui);
        /// reshape or set_passes({}) to author rendering from scratch.
        RenderGraph();

      public:
        RenderGraph(const RenderGraph&) = delete;
        RenderGraph& operator=(const RenderGraph&) = delete;
        RenderGraph(RenderGraph&&) = default;
        RenderGraph& operator=(RenderGraph&&) = default;

      public:
        /// @brief
        /// Purpose: Appends a pass to the end of the frame.
        void add_pass(RenderPass pass);

        /// @brief
        /// Purpose: The current pass list, in run order.
        const std::vector<RenderPass>& get_passes() const;

        /// @brief
        /// Purpose: Replaces the whole pass list (authoring rendering from scratch).
        void set_passes(std::vector<RenderPass> passes);

        /// @brief
        /// Purpose: Removes the first pass with the given name.
        void remove_pass(std::string_view name);

        /// @brief
        /// Purpose: Runs every pass in order — one full frame of rendering.
        void render(RenderContext& context);

      private:
        std::vector<RenderPass> _passes;
    };

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
