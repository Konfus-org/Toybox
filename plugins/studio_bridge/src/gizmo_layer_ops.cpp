#include "gizmo_layer_ops.h"
#include "engine_services.h"
#include "gizmo_layer_state.h"
#include "gizmo_op_replay.h"
#include "tags.h"
#include "wire.h"
#include "tbx/systems/graphics/camera_view.h"
#include "tbx/systems/graphics/frame_pass_context.h"
#include "tbx/systems/graphics/render_pass.h"
#include <algorithm>
#include <format>
#include <utility>

namespace tbx::studio_bridge
{
    //// WIRE PARSING ////

    // The layer address prefix — the engine half of the editor's "gizmos/{Name}" address template.
    static constexpr std::string_view LAYER_ADDRESS_PREFIX = "gizmos/";

    static bool parse_layer_address(const tbx::Json& params, std::string& out_name)
    {
        const auto address = params.value(Wire::ADDRESS, std::string());
        if (!address.starts_with(LAYER_ADDRESS_PREFIX) || address.size() <= LAYER_ADDRESS_PREFIX.size())
            return false;
        out_name = address.substr(LAYER_ADDRESS_PREFIX.size());
        return true;
    }

    //// SCOPE REBUILD ////

    static void rebuild_gizmo_scopes(GizmoLayerState& layers, const EngineServices& services)
    {
        // Group the visible layers by view scope, then replay each group into its scope's persistent
        // batch (creating batches for new scopes, dropping batches whose scope emptied). Layer order
        // within a scope is the map's name order — deterministic, so two overlapping layers never
        // flicker between frames.
        auto grouped = std::map<std::string, std::vector<const GizmoLayer*>>();
        for (const auto& [name, layer] : layers.layers)
            if (layer.is_visible && !layer.ops.empty())
                grouped[layer.view].push_back(&layer);

        std::erase_if(
            layers.batches, [&grouped](const auto& entry) { return !grouped.contains(entry.first); });

        auto scopes = std::vector<GizmoScope>();
        scopes.reserve(grouped.size());
        for (const auto& [view, group] : grouped)
        {
            auto& batch = layers.batches[view];
            if (!batch)
                batch = std::make_shared<tbx::Gizmos>(
                    services.graphics_backend, services.asset_manager);

            batch->clear();
            for (const auto* layer : group)
                replay_gizmo_ops(*batch, layer->ops);
            scopes.push_back(GizmoScope {view, batch});
        }

        auto lock = std::lock_guard(layers.scopes->mutex);
        layers.scopes->scopes = std::move(scopes);
    }

    //// GIZMO LAYER OPS ////

    void register_gizmo_layer_pass(GizmoLayerState& layers, const EngineServices& services)
    {
        auto rendering = services.rendering.lock();
        if (!rendering)
            return;

        if (!layers.scopes)
            layers.scopes = std::make_shared<GizmoScopeTable>();

        if (layers.pass.is_valid())
            rendering->remove_render_pass(layers.pass);

        // PassType::Overlay runs after the scene is composited; the editor.camera tag gates it to
        // editor viewports. The execute (render lane) copies the scope list under the table's mutex,
        // then draws every scope whose view is unrestricted or names this camera's view (editor
        // cameras carry their view name as a tag). The shared_ptr capture keeps the table — and
        // through it the batches — alive across an in-flight frame.
        auto execute = [table = layers.scopes](tbx::FramePassContext& context) -> tbx::Result
        {
            std::vector<GizmoScope> scopes = {};
            {
                auto lock = std::lock_guard(table->mutex);
                scopes = table->scopes;
            }
            if (scopes.empty())
                return tbx::Result::OK;

            const auto& tags = context.camera_view.tags;
            const auto view_projection = context.camera_view.camera.get_view_projection_matrix(
                context.camera_view.position, context.camera_view.rotation);
            for (const auto& scope : scopes)
            {
                if (!scope.view.empty() && std::ranges::find(tags, scope.view) == tags.end())
                    continue;
                scope.gizmos->render(context.backend, view_projection);
            }
            return tbx::Result::OK;
        };
        layers.pass = rendering->add_render_pass(std::make_shared<tbx::CallbackRenderPass>(
            tbx::PassType::Overlay,
            std::vector<std::string> {Tags::EDITOR_CAMERA},
            tbx::CallbackRenderPass::Callback(),
            std::move(execute)));
    }

    void unregister_gizmo_layer_pass(GizmoLayerState& layers, const EngineServices& services)
    {
        if (auto rendering = services.rendering.lock(); rendering && layers.pass.is_valid())
            rendering->remove_render_pass(layers.pass);
        layers.pass = {};
        if (layers.scopes)
        {
            auto lock = std::lock_guard(layers.scopes->mutex);
            layers.scopes->scopes.clear();
        }
        layers.scopes.reset();
        layers.batches.clear();
    }

    void submit_gizmo_layers(GizmoLayerState& layers, const EngineServices& services)
    {
        if (!layers.dirty || !layers.scopes)
            return;
        rebuild_gizmo_scopes(layers, services);
        layers.dirty = false;
    }

    tbx::Result set_gizmo_layer(GizmoLayerState& layers, const tbx::Json& params)
    {
        std::string name = {};
        if (!parse_layer_address(params, name))
            return tbx::Result(false, "gizmos.set: params carry no 'gizmos/<name>' address.");
        if (!params.contains(Wire::VALUE))
            return tbx::Result(false, "gizmos.set: params carry no value.");

        const auto key = params.value(Wire::KEY, std::string());
        const auto& value = params.at(Wire::VALUE);
        auto& layer = layers.layers[name];
        if (key == Wire::OPS)
        {
            if (!value.is_array())
                return tbx::Result(false, "gizmos.set: 'ops' must be an array of drawing ops.");
            layer.ops = value;
        }
        else if (key == Wire::IS_VISIBLE)
            layer.is_visible = value.is_boolean() ? value.get<bool>() : true;
        else if (key == Wire::VIEW)
            layer.view = value.is_string() ? value.get<std::string>() : std::string();
        else
            return tbx::Result(false, std::format("gizmos.set: unknown layer key '{}'.", key));

        layers.dirty = true;
        return tbx::Result::OK;
    }

    tbx::Result remove_gizmo_layer(GizmoLayerState& layers, const tbx::Json& params)
    {
        std::string name = {};
        if (!parse_layer_address(params, name))
            return tbx::Result(false, "gizmos.remove: params carry no 'gizmos/<name>' address.");

        layers.layers.erase(name);
        layers.dirty = true;
        return tbx::Result::OK;
    }
}
