#include "view_manager.h"
#include "asset_preview.h"
#include "bridge_utils.h"
#include "tags.h"
#include "wire.h"
#include "tbx/systems/debugging/logging.h"
#include "tbx/systems/debugging/macros.h"
#include "tbx/types/assets/world.h"
#include "tbx/types/components/camera.h"
#include "tbx/types/components/transform.h"
#include <algorithm>
#include <format>
#include <glm/glm.hpp>
#include <limits>
#include <memory>
#include <utility>
#include <vector>

namespace tbx::studio_bridge
{
    ViewManager::ViewManager(EngineServices& services)
        : _services(services)
    {
    }

    tbx::CameraView ViewManager::seed_camera_view(
        const tbx::RenderTexture& texture,
        bool editor_tag,
        bool copy_game_lens,
        bool seed_pose) const
    {
        auto view = tbx::CameraView();
        auto camera = tbx::Camera();

        // Editor and game cameras open aligned with the active world's game camera, so a new view
        // always opens somewhere useful (editor cameras then fly free and get re-aimed at the world once
        // geometry streams in; game cameras re-sync every frame). A preview view has no game camera, so
        // seed_pose is false and the orbit camera takes over on the first input update.
        if (seed_pose || copy_game_lens)
        {
            if (auto world = _services.get().active_world())
            {
                auto game_camera = world->first_with<tbx::Camera>();
                if (game_camera.get_id().is_valid())
                {
                    if (seed_pose && game_camera.has_component<tbx::Transform>())
                    {
                        const auto pose =
                            game_camera.get_component<tbx::Transform>().to_world_space(game_camera);
                        view.position = pose.position;
                        view.rotation = pose.rotation;
                    }
                    if (copy_game_lens && game_camera.has_component<tbx::Camera>())
                        camera = game_camera.get_component<tbx::Camera>();
                }
            }
        }

        camera.set_target(texture);
        view.camera = camera;
        // Editor views opt into the editor render passes (gizmo overlay + collider wireframes +
        // selection outline) via this tag; game and asset-preview views stay untagged and render exactly
        // what their camera sees.
        if (editor_tag)
            view.tags = {Tags::EDITOR_CAMERA};
        view.is_valid = true;
        return view;
    }

    std::pair<std::string, tbx::RenderTexture> ViewManager::make_view_target(float scale)
    {
        // A fresh index per view keeps every render texture (and its shared surface) distinct, so the
        // editor can stream and tear down each viewport independently.
        const auto index = _next_view_index++;
        const auto port = rpc_port();
        auto texture = tbx::RenderTexture(std::format("StudioView_{}_{}", port, index));
        // Default to the configured graphics resolution; a sub-1 scale renders cheaper for a small preview
        // (e.g. the browser hover card), with a floor so the texture never degenerates.
        auto size = _services.get().graphics_settings->get().resolution;
        if (scale > 0.0F && scale < 1.0F)
        {
            constexpr uint32 min_extent = 64U;
            size.width = std::max(min_extent, static_cast<uint32>(static_cast<float>(size.width) * scale));
            size.height = std::max(min_extent, static_cast<uint32>(static_cast<float>(size.height) * scale));
        }
        texture.size = size;
        return {std::format("ToyboxStudioFrame_{}_{}", port, index), std::move(texture)};
    }

    void ViewManager::register_and_add(
        std::unique_ptr<ViewStream> view,
        const std::shared_ptr<tbx::World>& world_override)
    {
        if (auto rendering = _services.get().rendering.lock())
            view->external_camera_id =
                rendering->register_external_camera({view->view, view->texture, world_override});

        {
            auto lock = std::lock_guard(_views_mutex);
            // Give the view a fresh input slot so the map mirrors the live views exactly (consumers can
            // assume an entry exists for every view they iterate).
            _view_inputs.emplace(view->name, ViewInput {});
            _views.push_back(std::move(view));
        }
        refresh_present_callback();
    }

    Result ViewManager::start_editor_view(std::string& out_name)
    {
        if (!_services.get().rendering.lock())
            return Result(false, "Rendering service is unavailable.");
        if (!_services.get().graphics_settings.has_value())
            return Result(false, "Graphics settings are unavailable.");

        auto view = std::make_unique<EditorViewStream>();
        auto [name, texture] = make_view_target();
        view->name = name;
        view->texture = std::move(texture);
        view->view = seed_camera_view(view->texture, /*editor_tag*/ true, /*game_lens*/ false, /*pose*/ true);

        out_name = name;
        register_and_add(std::move(view), nullptr);
        TBX_TRACE_INFO("StudioBridge: editor view started ('{}').", out_name);
        return Result::OK;
    }

    Result ViewManager::start_game_view(std::string& out_name)
    {
        if (!_services.get().rendering.lock())
            return Result(false, "Rendering service is unavailable.");
        if (!_services.get().graphics_settings.has_value())
            return Result(false, "Graphics settings are unavailable.");

        auto view = std::make_unique<GameViewStream>();
        auto [name, texture] = make_view_target();
        view->name = name;
        view->texture = std::move(texture);
        view->view = seed_camera_view(view->texture, /*editor_tag*/ false, /*game_lens*/ true, /*pose*/ true);

        out_name = name;
        register_and_add(std::move(view), nullptr);
        TBX_TRACE_INFO("StudioBridge: game view started ('{}').", out_name);
        return Result::OK;
    }

    Result ViewManager::start_asset_preview_view(
        uint32 asset_id, bool turntable, float render_scale, std::string& out_name, uint32& out_world_id)
    {
        if (!_services.get().rendering.lock())
            return Result(false, "Rendering service is unavailable.");
        if (!_services.get().graphics_settings.has_value())
            return Result(false, "Graphics settings are unavailable.");

        auto assets = _services.get().asset_manager.lock();
        if (!assets)
            return Result(false, "Asset manager is unavailable.");

        // Seed an isolated world with the shared base (key light + sky assets). The sky entity and the
        // previewed asset's entity are created by the editor through the world/entity API; the editor calls
        // view.frameAssetPreview once they exist so the orbit camera frames them.
        auto preview_world = std::make_shared<tbx::World>();
        seed_preview_world(*assets, *preview_world);

        auto view = std::make_unique<AssetPreviewViewStream>();
        auto [name, texture] = make_view_target(render_scale);
        view->name = name;
        view->texture = std::move(texture);
        view->view = seed_camera_view(view->texture, /*editor_tag*/ false, /*game_lens*/ false, /*pose*/ false);
        view->preview_world = preview_world;
        view->preview_asset_id = asset_id;
        view->auto_orbit = turntable;
        view->world_id = _next_world_id++;
        view->orbit_target = tbx::Vec3(0.0F);
        view->orbit_distance = 3.0F;

        out_name = name;
        out_world_id = view->world_id;
        register_and_add(std::move(view), preview_world);
        TBX_TRACE_INFO("StudioBridge: asset preview view started ('{}').", out_name);
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
            _view_inputs.erase(name);
            // GL/D3D teardown is only safe on the render lane, so hand the surface to the next present
            // callback rather than freeing it here on the main thread.
            _pending_shared_destroys.push_back(removed->texture);
        }

        if (auto rendering = _services.get().rendering.lock())
            rendering->unregister_external_camera(removed->external_camera_id);
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
            _view_inputs.clear();
            for (auto& view : removed)
                _pending_shared_destroys.push_back(view->texture);
        }

        if (auto rendering = _services.get().rendering.lock())
            for (auto& view : removed)
                rendering->unregister_external_camera(view->external_camera_id);
        refresh_present_callback();
        TBX_TRACE_INFO("StudioBridge: all views stopped.");
    }

    void ViewManager::refresh_present_callback()
    {
        auto rendering = _services.get().rendering.lock();
        if (!rendering)
            return;

        auto has_views = false;
        {
            auto lock = std::lock_guard(_views_mutex);
            has_views = !_views.empty();
        }

        // One callback fans every present out to whichever view owns that render target. Clearing it
        // when the last view goes away keeps the render lane free of editor work when nothing streams.
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

            if (view->surface_state == ViewSurfaceState::Pending)
            {
                // Create the shared surface on first present (here on the render lane), then tell the
                // editor its cross-process handle exactly once. The engine draws the view straight into
                // this texture from the next frame on.
                auto info = tbx::SharedTargetInfo();
                if (const auto result =
                        backend.create_shared_target(view->texture, backbuffer_size, info))
                {
                    view->shared = info;
                    view->surface_state = ViewSurfaceState::Ready;
                }
                else
                {
                    view->surface_state = ViewSurfaceState::Unavailable;
                    TBX_TRACE_ERROR(
                        "StudioBridge: GPU texture sharing unavailable for view '{}': {}",
                        view->name,
                        result.get_report());
                }

                // Announce either way; a zero handle tells the editor to show its empty ghost.
                auto params = tbx::Json::object();
                params[Wire::NAME] = view->name;
                params["sharedHandle"] = view->shared.shared_handle;
                params["width"] = view->shared.width;
                params["height"] = view->shared.height;
                params[Wire::FORMAT] = "bgra8";
                if (const auto host = _services.get().rpc_host.lock())
                    host->send_notification(Wire::VIEW_SURFACE, params);
            }
            else if (view->surface_state == ViewSurfaceState::Ready)
            {
                // The surface was created on an earlier present, so the engine has since drawn the first
                // real frame into it. Announce that exactly once so the editor knows loading is truly
                // done (an Unavailable surface never reaches Ready, so it never "presents").
                view->surface_state = ViewSurfaceState::Presented;
                auto params = tbx::Json::object();
                params[Wire::NAME] = view->name;
                if (const auto host = _services.get().rpc_host.lock())
                    host->send_notification(Wire::VIEW_PRESENTED, params);
            }
            return;
        }
    }

    void ViewManager::sync_game_cameras()
    {
        auto world = _services.get().active_world();
        if (!world)
            return;

        auto game_camera = world->first_with<tbx::Camera>();
        if (!game_camera.get_id().is_valid())
            return;
        const auto game_view = tbx::CameraView::from_entity(game_camera);
        if (!game_view.is_valid)
            return;

        auto lock = std::lock_guard(_views_mutex);
        for (auto& view : _views)
        {
            auto* game = dynamic_cast<GameViewStream*>(view.get());
            if (game == nullptr)
                continue;

            // Mirror the game camera's pose + lens + tags so the view shows exactly what the player
            // sees, but keep our own render target.
            game->view = game_view;
            game->view.camera.set_target(game->texture);
        }
    }

    void ViewManager::push_external_cameras()
    {
        auto rendering = _services.get().rendering.lock();
        if (!rendering)
            return;

        auto lock = std::lock_guard(_views_mutex);
        for (auto& view : _views)
        {
            // An asset-preview view renders its own isolated world; editor/game views render the active
            // world (null override).
            std::shared_ptr<tbx::World> world_override;
            if (auto* preview = dynamic_cast<AssetPreviewViewStream*>(view.get()))
                world_override = preview->preview_world;
            rendering->update_external_camera(
                view->external_camera_id,
                {view->view, view->texture, std::move(world_override)});
        }
    }

    void ViewManager::apply_view_input(const tbx::Json& params)
    {
        const auto name = params.value(Wire::VIEW, std::string());
        if (name.empty())
            return;

        auto lock = std::lock_guard(_views_mutex);
        // Only a live view (one with an input slot) accepts input; a late message for a stopped view is
        // dropped rather than leaking an orphan entry the consumers never read.
        const auto it = _view_inputs.find(name);
        if (it == _view_inputs.end())
            return;

        auto& input = it->second;
        input.focused = params.value("focused", false);
        input.buttons = params.value("buttons", 0U);
        // Mouse and wheel are deltas since the last message; accumulate until a frame consumes them.
        input.accumulated_mouse_dx += params.value("dx", 0.0F);
        input.accumulated_mouse_dy += params.value("dy", 0.0F);
        input.accumulated_wheel += params.value("wheel", 0.0F);
        // Normalized cursor in the rendered image (top-left origin), read by the gizmo.
        input.cursor_u = params.value("cursorU", 0.0F);
        input.cursor_v = params.value("cursorV", 0.0F);
        // Editor fly camera move-keys (absent for the other kinds → 0).
        input.move_keys = params.value("moveKeys", 0U);
        // Game view raw input for the engine input system (absent for the other kinds): pressed
        // tbx::InputKey codes and the absolute mouse position within the view.
        input.keys.clear();
        if (const auto keys = params.find("keys"); keys != params.end() && keys->is_array())
            for (const auto& key : *keys)
                if (key.is_number_integer())
                    input.keys.push_back(key.get<int>());
        input.mouse_x = params.value("mouseX", 0.0F);
        input.mouse_y = params.value("mouseY", 0.0F);
    }

    Result ViewManager::frame_asset_preview(uint32 world_id)
    {
        auto assets = _services.get().asset_manager.lock();
        if (!assets)
            return Result(false, "Asset manager is unavailable.");

        auto lock = std::lock_guard(_views_mutex);
        for (auto& view : _views)
        {
            auto* preview = dynamic_cast<AssetPreviewViewStream*>(view.get());
            if (preview == nullptr || preview->world_id != world_id || !preview->preview_world)
                continue;

            // Frame the orbit camera to the renderable bounds the editor built in this world (the previewed
            // entity), so it opens fully in view; fall back to a sensible default when nothing has bounds yet.
            auto minimum = glm::vec3(std::numeric_limits<float>::max());
            auto maximum = glm::vec3(std::numeric_limits<float>::lowest());
            auto any_bounds = false;
            for (auto& entity : preview->preview_world->get_all())
                any_bounds |= accumulate_entity_world_bounds(*assets, entity, minimum, maximum);

            if (any_bounds)
            {
                const auto center = (minimum + maximum) * 0.5F;
                const auto radius = glm::length(maximum - minimum) * 0.5F;
                preview->orbit_target = tbx::Vec3(center.x, center.y, center.z);
                preview->orbit_distance = std::max(radius * 2.5F, 1.0F);
            }
            else
            {
                preview->orbit_target = tbx::Vec3(0.0F);
                preview->orbit_distance = 3.0F;
            }
            return Result::OK;
        }
        return Result(false, "Unknown preview world.");
    }

    Result ViewManager::resolve_view_camera(const std::string& view_name, tbx::CameraView& out_camera) const
    {
        auto lock = std::lock_guard(_views_mutex);
        for (const auto& view : _views)
        {
            if (view->name != view_name)
                continue;
            out_camera = view->view;
            return out_camera.is_valid ? Result::OK : Result(false, "View camera is unavailable.");
        }
        return Result(false, "Unknown view.");
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
                if (auto* preview = dynamic_cast<AssetPreviewViewStream*>(view.get()))
                    return preview->preview_world;
                break;
            }
        }
        return _services.get().active_world();
    }

    std::shared_ptr<tbx::World> ViewManager::resolve_world_by_id(uint32 world_id) const
    {
        if (world_id == 0U)
            return nullptr;

        auto lock = std::lock_guard(_views_mutex);
        for (const auto& view : _views)
            if (auto* preview = dynamic_cast<AssetPreviewViewStream*>(view.get());
                preview != nullptr && preview->world_id == world_id)
                return preview->preview_world;
        return nullptr;
    }

    std::shared_ptr<tbx::World> ViewManager::find_preview_world_with(const tbx::Uuid& id) const
    {
        auto lock = std::lock_guard(_views_mutex);
        for (const auto& view : _views)
            if (auto* preview = dynamic_cast<AssetPreviewViewStream*>(view.get());
                preview != nullptr && preview->preview_world && preview->preview_world->has(id))
                return preview->preview_world;
        return nullptr;
    }
}
