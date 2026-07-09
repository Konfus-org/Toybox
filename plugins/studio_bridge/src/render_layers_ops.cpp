#include "render_layers_ops.h"
#include "engine_services.h"
#include "render_layers_state.h"
#include "tags.h"
#include "wire.h"
#include "tbx/systems/graphics/render_debug_view.h"
#include <string>

namespace tbx::studio_bridge
{
    // The wire stage names (the editor's camelCase enum values) to the engine's debug stage; an
    // unknown name reads as the normal frame rather than failing the whole push.
    static tbx::RenderDebugStage parse_stage(std::string_view name)
    {
        if (name == "diffuse")
            return tbx::RenderDebugStage::DIFFUSE;
        if (name == "normals")
            return tbx::RenderDebugStage::NORMALS;
        if (name == "shadows")
            return tbx::RenderDebugStage::SHADOWS;
        if (name == "depth")
            return tbx::RenderDebugStage::DEPTH;
        return tbx::RenderDebugStage::FINAL;
    }

    void set_render_layers(
        RenderLayersState& layers, const EngineServices& services, const tbx::Json& params)
    {
        layers.colliders_all = params.value(Wire::COLLIDERS_ALL, layers.colliders_all);
        layers.colliders_selected =
            params.value(Wire::COLLIDERS_SELECTED, layers.colliders_selected);
        layers.post_processing = params.value(Wire::POST_PROCESSING, layers.post_processing);
        if (const auto stage_it = params.find(Wire::STAGE); stage_it != params.end())
            layers.stage = parse_stage(stage_it->get<std::string>());

        // The post toggle + render stage live on the engine's pipeline, gated to editor cameras so
        // the game view keeps rendering normally beside a debugging editor viewport.
        if (auto rendering = services.rendering.lock())
            rendering->set_debug_view(tbx::RenderDebugView {
                .stage = layers.stage,
                .post_processing_enabled = layers.post_processing,
                .camera_tags = {Tags::EDITOR_CAMERA},
            });
    }
}
