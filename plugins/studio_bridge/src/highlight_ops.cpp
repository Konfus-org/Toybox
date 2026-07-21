#include "highlight_ops.h"
#include "engine_services.h"
#include "highlight_state.h"
#include "tags.h"
#include "tbx/systems/graphics/render_pass.h"
#include "tbx/types/components/post_processing.h"
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace tbx::studio_bridge
{
    void register_editor_highlights(HighlightState& state, const EngineServices& services)
    {
        auto rendering = services.rendering.lock();
        if (!rendering)
            return;
        if (state.pass.is_valid())
            rendering->remove_render_pass(state.pass);

        // The selection outline is declarative: a Post pass gated on the editor.camera tag carries
        // the effect, whose own entity-tag gate (editor.selected) makes it run — and feed the tag
        // mask — only while something is selected. No execute work of its own; the engine's post
        // chain runs the effect exactly like a world-authored one.
        auto pass = std::make_shared<tbx::RenderPass>(
            tbx::PassType::Post, std::vector<std::string> {Tags::EDITOR_CAMERA});
        pass->post_effects.push_back(tbx::PostProcessingEffect {
            .material = tbx::MaterialInstance(tbx::Handle("Materials/Gizmos/SelectionOutline.mat")),
            .is_enabled = true,
            .blend = 1.0F,
            .tags = {Tags::SELECTED}});
        state.pass = rendering->add_render_pass(std::move(pass));
    }

    void unregister_editor_highlights(HighlightState& state, const EngineServices& services)
    {
        if (auto rendering = services.rendering.lock(); rendering && state.pass.is_valid())
            rendering->remove_render_pass(state.pass);
        state.pass = {};
    }
}
