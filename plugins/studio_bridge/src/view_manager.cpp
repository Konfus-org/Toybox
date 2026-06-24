#include "view_manager.h"
#include "tbx/systems/debugging/logging.h"
#include "tbx/systems/debugging/macros.h"
#include "tbx/types/components/camera.h"
#include "tbx/types/components/transform.h"
#include "tbx/types/handle.h"
#include <algorithm>
#include <format>
#include <memory>
#include <utility>
#include <vector>

namespace tbx::studio_bridge
{
    ViewManager::ViewManager(EngineServices& services)
        : _services(services)
    {
        create_selection_overlay();
    }

    void ViewManager::create_selection_overlay()
    {
        // The selection outline is an editor concern, so the bridge owns it: a runtime entity carrying a
        // tag-gated PostProcessing effect. It lives in the view registry (never the game world), and its
        // effects are passed only to editor views — the engine renders them like any post effect without
        // knowing anything about editor selection.
        auto entity = tbx::Entity("EditorSelectionOverlay", _view_registry);

        auto outline = tbx::PostProcessingEffect();
        outline.material.material = tbx::Handle("Materials/Gizmos/SelectionOutline.mat");
        outline.tags = {SELECTION_TAG};

        auto post = tbx::PostProcessing();
        post.effects.push_back(std::move(outline));
        entity.add_component<tbx::PostProcessing>(post);

        _overlay_entity = entity.get_id();
    }

    std::vector<tbx::PostProcessingEffect> ViewManager::editor_overlay_effects() const
    {
        if (!_overlay_entity.is_valid())
            return {};

        auto entity = _view_registry.get(_overlay_entity);
        if (!entity.get_id().is_valid() || !entity.has_component<tbx::PostProcessing>())
            return {};
        return entity.get_component<tbx::PostProcessing>().effects;
    }

    Result ViewManager::start_view(bool is_game, std::string& out_name)
    {
        auto rendering = _services.rendering.lock();
        if (!rendering)
            return Result(false, "Rendering service is unavailable.");
        if (_services.graphics_settings == nullptr)
            return Result(false, "Graphics settings are unavailable.");

        auto world = _services.active_world();
        if (!world)
            return Result(false, "No active world to view.");

        // A fresh index per view keeps every render texture (and its shared surface) distinct, so
        // the editor can stream and tear down each viewport independently.
        const auto index = _next_view_index++;
        const auto port = rpc_port();
        auto view = std::make_unique<ViewStream>();
        view->name = std::format("ToyboxStudioFrame_{}_{}", port, index);
        view->is_game = is_game;

        view->texture = tbx::RenderTexture(std::format("StudioView_{}_{}", port, index));
        // Show the game at its real size: the view renders at the configured graphics resolution.
        view->texture.size = _services.graphics_settings->resolution;
        view->camera_id = create_view_camera(*world, view->texture, is_game);

        out_name = view->name;
        {
            auto lock = std::lock_guard(_views_mutex);
            _views.push_back(std::move(view));
        }

        refresh_present_callback();
        TBX_TRACE_INFO(
            "StudioBridge: {} view started ('{}').",
            is_game ? "game" : "editor",
            out_name);
        return Result::OK;
    }

    void ViewManager::stop_view(const std::string& name)
    {
        std::unique_ptr<ViewStream> removed;
        {
            auto lock = std::lock_guard(_views_mutex);
            const auto it = std::find_if(
                _views.begin(),
                _views.end(),
                [&name](const std::unique_ptr<ViewStream>& view)
                {
                    return view->name == name;
                });
            if (it == _views.end())
                return;

            removed = std::move(*it);
            _views.erase(it);
            // GL/D3D teardown is only safe on the render lane, so hand the surface to the next
            // present callback rather than freeing it here on the main thread.
            _pending_shared_destroys.push_back(removed->texture);

            // The camera destroy mutates _view_registry, which render_views reads on the render lane
            // under this same mutex. entt's per-call locks don't protect references read after the
            // call returns, so the mutation must happen under _views_mutex too. destroy_view_camera
            // only touches _view_registry (no nested lock), so holding the lock here cannot deadlock
            // against render_views.
            destroy_view_camera(removed->camera_id);
        }

        refresh_present_callback();
        TBX_TRACE_INFO("StudioBridge: view stopped ('{}').", removed->name);
    }

    void ViewManager::stop_all_views()
    {
        std::vector<std::unique_ptr<ViewStream>> removed;
        {
            auto lock = std::lock_guard(_views_mutex);
            if (_views.empty())
                return;

            removed = std::move(_views);
            _views.clear();
            for (auto& view : removed)
                _pending_shared_destroys.push_back(view->texture);

            // Destroy the cameras under the lock for the same reason as stop_view: the registry
            // mutation must not race render_views' registry reads on the render lane.
            for (auto& view : removed)
                destroy_view_camera(view->camera_id);
        }

        refresh_present_callback();
        TBX_TRACE_INFO("StudioBridge: all views stopped.");
    }

    void ViewManager::refresh_present_callback()
    {
        auto rendering = _services.rendering.lock();
        if (!rendering)
            return;

        auto has_views = false;
        {
            auto lock = std::lock_guard(_views_mutex);
            has_views = !_views.empty();
        }

        // One callback fans every present out to whichever view owns that render target. Clearing
        // it when the last view goes away keeps the render lane free of editor work when nothing is
        // streaming.
        if (has_views)
        {
            rendering->set_pre_present_callback(
                [this](
                    tbx::IGraphicsBackend& backend,
                    const tbx::RenderTarget& output_target,
                    const tbx::Size& backbuffer_size)
                {
                    ensure_view_surfaces(backend, output_target, backbuffer_size);
                });
        }
        else
        {
            rendering->set_pre_present_callback({});
        }
    }

    void ViewManager::ensure_view_surfaces(
        tbx::IGraphicsBackend& backend,
        const tbx::RenderTarget& output_target,
        const tbx::Size& backbuffer_size)
    {
        auto lock = std::lock_guard(_views_mutex);

        // Free the surfaces of views that have stopped. Any view's present drains the queue, so a
        // stopped view's GPU texture is reclaimed as soon as another view renders (or at shutdown).
        for (const auto& texture : _pending_shared_destroys)
            backend.destroy_shared_target(texture);
        _pending_shared_destroys.clear();

        for (auto& view : _views)
        {
            if (output_target.id != view->texture.id)
                continue;

            // Create the shared surface on first render (here on the render lane), then tell the
            // editor its cross-process handle exactly once. begin_frame draws the view straight
            // into this texture from the next frame on.
            if (!view->shared_attempted)
            {
                view->shared_attempted = true;
                auto info = tbx::SharedTargetInfo();
                if (const auto result =
                        backend.create_shared_target(view->texture, backbuffer_size, info))
                {
                    view->shared = info;
                    view->shared_ready = true;
                }
                else
                {
                    TBX_TRACE_ERROR(
                        "StudioBridge: GPU texture sharing unavailable for view '{}': {}",
                        view->name,
                        result.get_report());
                }

                // Announce either way; a zero handle tells the editor to show its empty ghost.
                auto params = tbx::Json::object();
                params["name"] = view->name;
                params["sharedHandle"] = view->shared.shared_handle;
                params["width"] = view->shared.width;
                params["height"] = view->shared.height;
                params["format"] = "bgra8";
                if (const auto host = _services.rpc_host.lock())
                    host->send_notification("view.surface", params);
            }
            else if (view->shared_ready && !view->presented_announced)
            {
                // The surface was created on an earlier present, so render_views has since drawn
                // the first real frame into it. Announce that exactly once so the editor knows
                // loading is truly done (a failed share never sets shared_ready, so it never
                // "presents").
                view->presented_announced = true;
                auto params = tbx::Json::object();
                params["name"] = view->name;
                if (const auto host = _services.rpc_host.lock())
                    host->send_notification("view.presented", params);
            }
            // Gizmos/selection are drawn by the engine's gizmo pass (see render_views' overlay),
            // not here — this callback only manages the shared surface lifecycle.
            return;
        }
    }

    tbx::Entity ViewManager::find_first_game_camera(tbx::World& world) const
    {
        // The world holds only the user's entities now (view cameras live in _view_registry), so the
        // game camera is simply its first camera.
        return world.first_with<tbx::Camera>();
    }

    tbx::Uuid ViewManager::create_view_camera(
        tbx::World& world,
        const tbx::RenderTexture& texture,
        bool is_game)
    {
        // Both editor and game cameras start aligned with the world's own (game) camera, so a new
        // view always opens somewhere useful. Editor cameras then move freely; game cameras get
        // re-synced to the game camera every frame in sync_game_views().
        auto initial_transform = tbx::Transform();
        auto game_camera = find_first_game_camera(world);
        if (game_camera.get_id().is_valid() && game_camera.has_component<tbx::Transform>())
            initial_transform =
                game_camera.get_component<tbx::Transform>().to_world_space(game_camera);
        initial_transform.id = tbx::Uuid::generate();

        // The editor camera keeps the game camera's position, but its direction is set later (in
        // InputController::update_editor_cameras) to face the world once geometry has streamed in —
        // facing the game camera's authored direction would often open the view looking at empty sky
        // on games that orient their camera on play.

        // View cameras are tooling owned by the studio bridge, so they are created in the plugin's
        // own registry — never in the game world — and so never appear in the world or the play
        // snapshot.
        auto camera_entity =
            tbx::Entity(is_game ? "GameViewCamera" : "EditorCamera", _view_registry);
        camera_entity.add_component<tbx::Transform>(initial_transform);

        auto camera = tbx::Camera();
        if (is_game && game_camera.get_id().is_valid() && game_camera.has_component<tbx::Camera>())
            camera = game_camera.get_component<tbx::Camera>();
        camera.set_target(texture);
        camera_entity.add_component<tbx::Camera>(camera);
        return camera_entity.get_id();
    }

    void ViewManager::destroy_view_camera(const tbx::Uuid& camera_id)
    {
        if (!camera_id.is_valid())
            return;

        auto camera_entity = _view_registry.get(camera_id);
        if (camera_entity.get_id().is_valid())
            _view_registry.remove(camera_entity);
    }

    void ViewManager::render_views(const tbx::DeltaTime& dt, const SubmitOverlayFn& submit_overlay)
    {
        auto rendering = _services.rendering.lock();
        if (!rendering || _services.graphics_settings == nullptr)
            return;

        auto lock = std::lock_guard(_views_mutex);

        // Pick the editor view that drives the transform overlay: the focused one if any (so its
        // hover/drag highlight shows), else the first editor view so the gizmo is still visible on the
        // selection before the viewport is focused — e.g. right after switching tools from the toolbar,
        // which leaves the viewport unfocused. Geometry is world-space, so submitting the overlay once
        // is correct in every view.
        const ViewStream* overlay = nullptr;
        auto overlay_camera = tbx::CameraView();
        for (const auto& view : _views)
        {
            if (view->is_game || !view->camera_id.is_valid())
                continue;
            auto camera_entity = _view_registry.get(view->camera_id);
            if (!camera_entity.get_id().is_valid())
                continue;
            const auto camera_view = tbx::CameraView::from_entity(camera_entity);
            if (!camera_view.is_valid)
                continue;

            // The focused view wins outright; otherwise keep the first valid one as the fallback.
            if (view->focused)
            {
                overlay = view.get();
                overlay_camera = camera_view;
                break;
            }
            if (overlay == nullptr)
            {
                overlay = view.get();
                overlay_camera = camera_view;
            }
        }

        submit_overlay(
            overlay != nullptr ? &overlay_camera : nullptr,
            overlay != nullptr ? &overlay->gizmo : nullptr);

        // Our view cameras are not in the game world, so the engine's render loop never draws them.
        // Render each one ourselves into its own texture; Rendering::render draws the active game world
        // from whatever camera view it is given, regardless of which registry the camera lives in.
        // Editor views also get the editor's overlay post effects (selection outline); game views show
        // exactly what the player sees, so they don't.
        const auto overlay_effects = editor_overlay_effects();
        for (auto& view : _views)
        {
            if (!view->camera_id.is_valid())
                continue;

            auto camera_entity = _view_registry.get(view->camera_id);
            if (!camera_entity.get_id().is_valid())
                continue;

            const auto camera_view = tbx::CameraView::from_entity(camera_entity);
            if (!camera_view.is_valid)
                continue;

            rendering->render(
                dt,
                *_services.graphics_settings,
                camera_view,
                camera_view.camera.get_render_target(),
                view->is_game ? std::vector<tbx::PostProcessingEffect>() : overlay_effects);
        }
    }

    void ViewManager::sync_game_views()
    {
        auto lock = std::lock_guard(_views_mutex);
        const auto has_game_view = std::any_of(
            _views.begin(),
            _views.end(),
            [](const std::unique_ptr<ViewStream>& view)
            {
                return view->is_game;
            });
        if (!has_game_view)
            return;

        auto world = _services.active_world();
        if (!world)
            return;

        auto game_camera = find_first_game_camera(*world);
        if (!game_camera.get_id().is_valid())
            return;

        const auto has_transform = game_camera.has_component<tbx::Transform>();
        const auto has_camera = game_camera.has_component<tbx::Camera>();
        for (auto& view : _views)
        {
            if (!view->is_game || !view->camera_id.is_valid())
                continue;

            auto mirror = _view_registry.get(view->camera_id);
            if (!mirror.get_id().is_valid())
                continue;

            // Mirror the game camera's pose and lens so the view shows exactly what the player
            // sees, but keep our own render target and a stable transform id.
            if (has_transform && mirror.has_component<tbx::Transform>())
            {
                auto& mirror_transform = mirror.get_component<tbx::Transform>();
                const auto transform_id = mirror_transform.id;
                mirror_transform =
                    game_camera.get_component<tbx::Transform>().to_world_space(game_camera);
                mirror_transform.id = transform_id;
            }

            if (has_camera && mirror.has_component<tbx::Camera>())
            {
                auto& mirror_camera = mirror.get_component<tbx::Camera>();
                mirror_camera = game_camera.get_component<tbx::Camera>();
                mirror_camera.set_target(view->texture);
            }
        }
    }

    void ViewManager::apply_view_input(const tbx::Json& params)
    {
        const auto name = params.value("view", std::string());
        if (name.empty())
            return;

        auto lock = std::lock_guard(_views_mutex);
        for (auto& view : _views)
        {
            if (view->name != name)
                continue;

            view->focused = params.value("focused", false);
            view->buttons = params.value("buttons", 0U);
            view->move_keys = params.value("moveKeys", 0U);
            // Mouse and wheel are deltas since the last message; accumulate until a frame consumes
            // them.
            view->accumulated_mouse_dx += params.value("dx", 0.0F);
            view->accumulated_mouse_dy += params.value("dy", 0.0F);
            view->accumulated_wheel += params.value("wheel", 0.0F);

            // Game views also carry raw input for the engine input system: pressed tbx::InputKey
            // codes and the absolute mouse position within the view.
            view->keys.clear();
            if (const auto keys = params.find("keys"); keys != params.end() && keys->is_array())
            {
                for (const auto& key : *keys)
                {
                    if (key.is_number_integer())
                        view->keys.push_back(key.get<int>());
                }
            }
            view->mouse_x = params.value("mouseX", 0.0F);
            view->mouse_y = params.value("mouseY", 0.0F);

            // Normalized cursor in the rendered image (top-left origin), used to drive the gizmo.
            view->gizmo.cursor_u = params.value("cursorU", 0.0F);
            view->gizmo.cursor_v = params.value("cursorV", 0.0F);
            return;
        }
    }

    Result ViewManager::resolve_view_camera(const std::string& view_name, tbx::CameraView& out_camera)
    {
        // Snapshot the view's camera id under the lock, then resolve it outside.
        auto camera_id = tbx::Uuid();
        {
            auto lock = std::lock_guard(_views_mutex);
            for (const auto& view : _views)
            {
                if (view->name == view_name)
                {
                    camera_id = view->camera_id;
                    break;
                }
            }
        }
        if (!camera_id.is_valid())
            return Result(false, "Unknown view.");

        auto camera_entity = _view_registry.get(camera_id);
        out_camera = tbx::CameraView::from_entity(camera_entity);
        if (!out_camera.is_valid)
            return Result(false, "View camera is unavailable.");

        return Result::OK;
    }
}
