#include "view_manager.h"
#include "asset_preview.h"
#include "tbx/systems/debugging/logging.h"
#include "tbx/systems/debugging/macros.h"
#include "tbx/types/assets/world.h"
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

    Result ViewManager::start_view(ViewKind kind, uint32 asset_id, std::string& out_name)
    {
        auto rendering = _services.rendering.lock();
        if (!rendering)
            return Result(false, "Rendering service is unavailable.");
        if (_services.graphics_settings == nullptr)
            return Result(false, "Graphics settings are unavailable.");

        // An AssetPreview view builds and owns an isolated world holding just the previewed asset;
        // Editor/Game views draw the active world.
        std::shared_ptr<tbx::World> preview_world;
        auto framing = AssetPreviewFraming();
        if (kind == ViewKind::AssetPreview)
        {
            auto assets = _services.asset_manager.lock();
            if (!assets)
                return Result(false, "Asset manager is unavailable.");
            preview_world = std::make_shared<tbx::World>();
            // Empty option/skybox use the type defaults (sphere/plane/metal presentation, day sky);
            // the editor can change them later via set_preview_option / set_preview_skybox.
            if (!build_asset_preview(
                    *assets, asset_id, std::string(), std::string(), *preview_world, framing))
                return Result(false, "Asset cannot be previewed.");
        }

        // The camera is created against (and an asset preview renders) the preview world when present,
        // otherwise the active world.
        auto camera_world = preview_world ? preview_world : _services.active_world();
        if (!camera_world)
            return Result(false, "No world to view.");

        // A fresh index per view keeps every render texture (and its shared surface) distinct, so
        // the editor can stream and tear down each viewport independently.
        const auto index = _next_view_index++;
        const auto port = rpc_port();
        auto view = std::make_unique<ViewStream>();
        view->name = std::format("ToyboxStudioFrame_{}_{}", port, index);
        view->kind = kind;
        view->preview_world = preview_world;
        view->preview_asset_id = asset_id;

        view->texture = tbx::RenderTexture(std::format("StudioView_{}_{}", port, index));
        // Show the game at its real size: the view renders at the configured graphics resolution.
        view->texture.size = _services.graphics_settings->resolution;
        view->camera_id = create_view_camera(*camera_world, view->texture, kind);
        if (kind == ViewKind::AssetPreview)
        {
            view->orbit_target = framing.target;
            view->orbit_distance = framing.distance;
        }

        out_name = view->name;
        {
            auto lock = std::lock_guard(_views_mutex);
            _views.push_back(std::move(view));
        }

        refresh_present_callback();
        const auto* kind_name = kind == ViewKind::Game            ? "game"
                                : kind == ViewKind::AssetPreview  ? "asset preview"
                                                                  : "editor";
        TBX_TRACE_INFO("StudioBridge: {} view started ('{}').", kind_name, out_name);
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
        ViewKind kind)
    {
        const bool is_game = kind == ViewKind::Game;

        // Editor and game cameras start aligned with the world's own (game) camera, so a new view
        // always opens somewhere useful. Editor cameras then move freely; game cameras get re-synced
        // to the game camera every frame in sync_game_views(). An asset-preview world has no camera,
        // so its initial transform stays at the origin and is overwritten by the orbit camera on the
        // first input update.
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
        const auto* camera_name = kind == ViewKind::Game            ? "GameViewCamera"
                                  : kind == ViewKind::AssetPreview  ? "AssetPreviewCamera"
                                                                    : "EditorCamera";
        auto camera_entity = tbx::Entity(camera_name, _view_registry);
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
            // Only editor views carry the transform gizmo; game and asset-preview views don't.
            if (view->kind != ViewKind::Editor || !view->camera_id.is_valid())
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

            // Game views show exactly what the player sees (no editor overlay); editor and asset-
            // preview views get the selection-outline overlay. An asset-preview view renders its own
            // isolated world (preview_world); editor/game views pass null and draw the active world.
            rendering->render(
                dt,
                *_services.graphics_settings,
                camera_view,
                camera_view.camera.get_render_target(),
                view->kind == ViewKind::Game ? std::vector<tbx::PostProcessingEffect>()
                                             : overlay_effects,
                view->preview_world,
                // Only editor views draw the gizmo overlay (transform handles + collider
                // wireframes); game and asset-preview views show exactly what their camera sees.
                view->kind == ViewKind::Editor);
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
                return view->kind == ViewKind::Game;
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
            if (view->kind != ViewKind::Game || !view->camera_id.is_valid())
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

    Result ViewManager::set_preview_option(const std::string& view_name, const std::string& option)
    {
        {
            auto lock = std::lock_guard(_views_mutex);
            for (auto& view : _views)
            {
                if (view->name != view_name)
                    continue;
                if (view->kind != ViewKind::AssetPreview)
                    return Result(false, "View is not an asset preview.");
                view->preview_option = option;
                break;
            }
        }
        return rebuild_preview(view_name);
    }

    Result ViewManager::set_preview_skybox(const std::string& view_name, const std::string& skybox)
    {
        {
            auto lock = std::lock_guard(_views_mutex);
            for (auto& view : _views)
            {
                if (view->name != view_name)
                    continue;
                if (view->kind != ViewKind::AssetPreview)
                    return Result(false, "View is not an asset preview.");
                view->preview_skybox = skybox;
                break;
            }
        }
        return rebuild_preview(view_name);
    }

    Result ViewManager::rebuild_preview(const std::string& view_name)
    {
        auto assets = _services.asset_manager.lock();
        if (!assets)
            return Result(false, "Asset manager is unavailable.");

        // Snapshot the view's asset id + current options under the lock, then build the new world
        // OUTSIDE it so the (cached) asset load can't stall the render lane, which also takes this
        // mutex each frame.
        auto asset_id = uint32(0);
        auto option = std::string();
        auto skybox = std::string();
        auto found = false;
        {
            auto lock = std::lock_guard(_views_mutex);
            for (const auto& view : _views)
            {
                if (view->name != view_name || view->kind != ViewKind::AssetPreview)
                    continue;
                asset_id = view->preview_asset_id;
                option = view->preview_option;
                skybox = view->preview_skybox;
                found = true;
                break;
            }
        }
        if (!found)
            return Result(false, "Unknown view.");

        auto new_world = std::make_shared<tbx::World>();
        auto framing = AssetPreviewFraming();
        if (!build_asset_preview(*assets, asset_id, option, skybox, *new_world, framing))
            return Result(false, "Asset cannot be previewed with that option.");

        // Swap the world in under the lock; an in-flight frame keeps the old world alive via its
        // captured shared_ptr, and the orbit camera (in the view registry) is left untouched so the
        // view doesn't jump. Re-find the view in case it was stopped while we built.
        auto lock = std::lock_guard(_views_mutex);
        for (auto& view : _views)
        {
            if (view->name == view_name && view->kind == ViewKind::AssetPreview)
            {
                view->preview_world = std::move(new_world);
                return Result::OK;
            }
        }
        return Result(false, "View is no longer available.");
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

    std::shared_ptr<tbx::World> ViewManager::resolve_view_world(const std::string& view_name) const
    {
        {
            auto lock = std::lock_guard(_views_mutex);
            for (const auto& view : _views)
            {
                if (view->name != view_name)
                    continue;
                // An asset-preview view draws and picks against its isolated world; every other view
                // uses the active world.
                if (view->kind == ViewKind::AssetPreview)
                    return view->preview_world;
                break;
            }
        }
        return _services.active_world();
    }

    std::shared_ptr<tbx::World> ViewManager::find_preview_world_with(const tbx::Uuid& id) const
    {
        auto lock = std::lock_guard(_views_mutex);
        for (const auto& view : _views)
        {
            if (view->kind == ViewKind::AssetPreview && view->preview_world
                && view->preview_world->has(id))
                return view->preview_world;
        }
        return nullptr;
    }
}
