#include "view_ops.h"
#include "bridge_utils.h"
#include "engine_services.h"
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
    // The RPC port (from the host), woven into each view/texture name so concurrent editors stay
    // distinct. 0 until the host is bound + listening.
    static uint16 rpc_port(const EngineServices& services)
    {
        const auto host = services.rpc_host.lock();
        return host ? host->port() : 0U;
    }

    // The non-active world registered under `world_id` (null for id 0 or an unknown id). Assumes the
    // caller already holds views.mutex — used inside the per-frame camera loops that hold it.
    static std::shared_ptr<tbx::World> world_by_id_locked(const ViewState& views, uint32 world_id)
    {
        if (world_id == 0U)
            return nullptr;
        const auto it = views.worlds.find(world_id);
        return it == views.worlds.end() ? nullptr : it->second;
    }

    // The actual world a view backed by `world_id` reads (its game camera, its focus): the active world
    // for id 0, else the registry world. Takes views.mutex, so call it OUTSIDE a locked section.
    static std::shared_ptr<tbx::World> world_for_stream(
        ViewState& views, const EngineServices& services, uint32 world_id)
    {
        return world_id == 0U ? services.active_world() : resolve_world_by_id(views, world_id);
    }

    // Builds an initial camera view for a new view's texture. Editor/game views open aligned with
    // the active world's game camera (game views also copy its lens); preview views open at the
    // origin (the orbit camera takes over). Editor views carry the editor-camera tag so the gizmo /
    // collider / selection passes apply.
    static tbx::CameraView seed_camera_view(
        const std::shared_ptr<tbx::World>& world,
        const tbx::RenderTexture& texture,
        bool editor_tag,
        bool copy_game_lens,
        bool seed_pose)
    {
        auto view = tbx::CameraView();
        auto camera = tbx::Camera();

        // Editor and game cameras open aligned with their world's game camera, so a new view always opens
        // somewhere useful (editor cameras then fly free and get re-aimed at the world once geometry
        // streams in; game cameras re-sync every frame). A preview view has no game camera, so seed_pose
        // is false and the orbit camera takes over on the first input update.
        if ((seed_pose || copy_game_lens) && world)
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

    // Generates a unique view name + a render texture sized to the graphics resolution, optionally scaled
    // down (0 < scale < 1) for a cheaper low-resolution view such as the browser's small hover preview.
    static std::pair<std::string, tbx::RenderTexture> make_view_target(
        ViewState& views, const EngineServices& services, float scale = 1.0F)
    {
        // A fresh index per view keeps every render texture (and its shared surface) distinct, so the
        // editor can stream and tear down each viewport independently.
        const auto index = views.next_view_index++;
        const auto port = rpc_port(services);
        auto texture = tbx::RenderTexture(std::format("StudioView_{}_{}", port, index));
        // Default to the configured graphics resolution; a sub-1 scale renders cheaper for a small preview
        // (e.g. the browser hover card), with a floor so the texture never degenerates.
        auto size = services.graphics_settings->get().resolution;
        if (scale > 0.0F && scale < 1.0F)
        {
            constexpr uint32 min_extent = 64U;
            size.width = std::max(min_extent, static_cast<uint32>(static_cast<float>(size.width) * scale));
            size.height = std::max(min_extent, static_cast<uint32>(static_cast<float>(size.height) * scale));
        }
        texture.size = size;
        return {std::format("ToyboxStudioFrame_{}_{}", port, index), std::move(texture)};
    }

    // Render-lane present callback body: frees stopped views' shared surfaces, creates a view's
    // surface on first present, and announces its cross-process handle (then its first frame) to
    // the editor.
    static void ensure_view_surfaces(
        ViewState& views,
        const EngineServices& services,
        tbx::IGraphicsBackend& backend,
        const tbx::RenderTarget& output_target,
        const tbx::Size& backbuffer_size)
    {
        auto lock = std::lock_guard(views.mutex);

        // Free the surfaces of views that have stopped. Any view's present drains the queue, so a
        // stopped view's GPU texture is reclaimed as soon as another view renders (or at shutdown).
        for (const auto& texture : views.pending_shared_destroys)
            backend.destroy_shared_target(texture);
        views.pending_shared_destroys.clear();

        for (auto& view : views.streams)
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
                if (const auto host = services.rpc_host.lock())
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
                if (const auto host = services.rpc_host.lock())
                    host->send_notification(Wire::VIEW_PRESENTED, params);
            }
            return;
        }
    }

    static void refresh_present_callback(ViewState& views, const EngineServices& services)
    {
        auto rendering = services.rendering.lock();
        if (!rendering)
            return;

        auto has_views = false;
        {
            auto lock = std::lock_guard(views.mutex);
            has_views = !views.streams.empty();
        }

        // One callback fans every present out to whichever view owns that render target. Clearing it
        // when the last view goes away keeps the render lane free of editor work when nothing streams.
        // The captured references are the plugin's own ViewState + EngineServices, which outlive the
        // callback: stop_all_views clears it on teardown before the plugin state goes away.
        if (has_views)
        {
            rendering->set_pre_present_callback(
                [&views, &services](
                    tbx::IGraphicsBackend& backend,
                    const tbx::RenderTarget& output_target,
                    const tbx::Size& backbuffer_size)
                {
                    ensure_view_surfaces(views, services, backend, output_target, backbuffer_size);
                });
        }
        else
        {
            rendering->set_pre_present_callback({});
        }
    }

    // Registers the (type-built, camera-seeded) view's external camera with the engine and adds it
    // to the collection. world_override is the preview world (null = active world).
    static void register_and_add(
        ViewState& views,
        const EngineServices& services,
        std::unique_ptr<ViewStream> view,
        const std::shared_ptr<tbx::World>& world_override)
    {
        if (auto rendering = services.rendering.lock())
            view->external_camera_id =
                rendering->register_external_camera({view->view, view->texture, world_override});

        {
            auto lock = std::lock_guard(views.mutex);
            // Give the view a fresh input slot so the map mirrors the live views exactly (consumers can
            // assume an entry exists for every view they iterate).
            views.inputs.emplace(view->name, ViewInput {});
            views.streams.push_back(std::move(view));
        }
        refresh_present_callback(views, services);
    }

    // Uniform render scale for an editor viewport sized to its on-screen pane: the render target is the
    // graphics resolution scaled by (pane long edge / resolution long edge), so a small pane renders
    // fewer pixels instead of a full-resolution frame it then downscales — capped so it never exceeds the
    // configured resolution and floored so it never gets too soft. A zero/unknown pane size renders full.
    static float editor_view_scale(const EngineServices& services, uint32 pane_width, uint32 pane_height)
    {
        constexpr float MIN_EDITOR_VIEW_SCALE = 0.5F;
        const auto resolution = services.graphics_settings->get().resolution;
        const uint32 resolution_long = std::max(resolution.width, resolution.height);
        const uint32 pane_long = std::max(pane_width, pane_height);
        if (pane_long == 0U || resolution_long == 0U)
            return 1.0F;
        return std::clamp(
            static_cast<float>(pane_long) / static_cast<float>(resolution_long),
            MIN_EDITOR_VIEW_SCALE,
            1.0F);
    }

    Result start_editor_view(
        ViewState& views, const EngineServices& services, uint32 world_id, uint32 pane_width,
        uint32 pane_height, std::string& out_name)
    {
        if (!services.rendering.lock())
            return Result(false, "Rendering service is unavailable.");
        if (!services.graphics_settings.has_value())
            return Result(false, "Graphics settings are unavailable.");

        // The world this view renders (the active world for id 0, else a loaded world). A non-zero id
        // that names no registered world is a stale/racing bind — fail rather than silently show active.
        const auto world = world_for_stream(views, services, world_id);
        if (world_id != 0U && !world)
            return Result(false, "Editor view bound to an unknown world id.");

        auto view = std::make_unique<EditorViewStream>();
        auto [name, texture] =
            make_view_target(views, services, editor_view_scale(services, pane_width, pane_height));
        view->name = name;
        view->texture = std::move(texture);
        view->world_id = world_id;
        view->view = seed_camera_view(
            world, view->texture, /*editor_tag*/ true, /*game_lens*/ false, /*pose*/ true);
        // Editor cameras also advertise which view they render: the gizmo layer store's view-restricted
        // scopes match this tag, so an editor-authored layer can draw in one viewport only.
        view->view.tags.push_back(name);

        out_name = name;
        register_and_add(views, services, std::move(view), world_id == 0U ? nullptr : world);
        TBX_TRACE_INFO("StudioBridge: editor view started ('{}', world {}).", out_name, world_id);
        return Result::OK;
    }

    Result start_game_view(
        ViewState& views, const EngineServices& services, uint32 world_id, std::string& out_name)
    {
        if (!services.rendering.lock())
            return Result(false, "Rendering service is unavailable.");
        if (!services.graphics_settings.has_value())
            return Result(false, "Graphics settings are unavailable.");

        const auto world = world_for_stream(views, services, world_id);
        if (world_id != 0U && !world)
            return Result(false, "Game view bound to an unknown world id.");

        auto view = std::make_unique<GameViewStream>();
        auto [name, texture] = make_view_target(views, services);
        view->name = name;
        view->texture = std::move(texture);
        view->world_id = world_id;
        view->view = seed_camera_view(
            world, view->texture, /*editor_tag*/ false, /*game_lens*/ true, /*pose*/ true);

        out_name = name;
        register_and_add(views, services, std::move(view), world_id == 0U ? nullptr : world);
        TBX_TRACE_INFO("StudioBridge: game view started ('{}', world {}).", out_name, world_id);
        return Result::OK;
    }

    Result start_asset_preview_view(
        ViewState& views,
        const EngineServices& services,
        uint32 world_id,
        bool turntable,
        float render_scale,
        std::string& out_name)
    {
        if (!services.rendering.lock())
            return Result(false, "Rendering service is unavailable.");
        if (!services.graphics_settings.has_value())
            return Result(false, "Graphics settings are unavailable.");

        // The editor loads the preview world (world.load) and force-registers its dependency assets
        // (asset.load) itself, then binds this orbit view to that world — the bridge owns no preview
        // content. Resolve the editor-provided world; a preview needs a real loaded world (id 0 is the
        // active editing world, never a preview). The previewed entity is built by the editor through the
        // world/entity API, and it calls view.frameAssetPreview once that exists so the camera frames it.
        if (world_id == 0U)
            return Result(false, "An asset-preview view needs a preview world id.");
        auto preview_world = world_for_stream(views, services, world_id);
        if (!preview_world)
            return Result(false, "Asset-preview view bound to an unknown world id.");

        auto view = std::make_unique<AssetPreviewViewStream>();
        auto [name, texture] = make_view_target(views, services, render_scale);
        view->name = name;
        view->texture = std::move(texture);
        view->view = seed_camera_view(
            nullptr, view->texture, /*editor_tag*/ false, /*game_lens*/ false, /*pose*/ false);
        view->preview_world = preview_world;
        view->auto_orbit = turntable;
        view->world_id = world_id;
        view->owns_world = false; // the editor loaded this world (world.load) and releases it (world.close)
        view->orbit_target = tbx::Vec3(0.0F);
        view->orbit_distance = 3.0F;

        out_name = name;
        register_and_add(views, services, std::move(view), preview_world);
        TBX_TRACE_INFO("StudioBridge: asset preview view started ('{}', world {}).", name, world_id);
        return Result::OK;
    }

    void stop_view(ViewState& views, const EngineServices& services, const std::string& name)
    {
        std::unique_ptr<ViewStream> removed;
        auto owned_world = tbx::Uuid();
        {
            auto lock = std::lock_guard(views.mutex);
            const auto it = std::find_if(
                views.streams.begin(),
                views.streams.end(),
                [&name](const std::unique_ptr<ViewStream>& view)
                {
                    return view->name == name;
                });
            if (it == views.streams.end())
                return;

            removed = std::move(*it);
            views.streams.erase(it);
            views.inputs.erase(name);
            // A view that owns its world (a preview) releases it as it stops; a view that merely references
            // a bound world leaves it for its owner (world.close). Capture the world's id to close after
            // the lock is released.
            if (removed->owns_world)
                if (const auto world = views.worlds.find(removed->world_id); world != views.worlds.end())
                {
                    if (world->second)
                        owned_world = world->second->id;
                    views.worlds.erase(world);
                }
            // GL/D3D teardown is only safe on the render lane, so hand the surface to the next present
            // callback rather than freeing it here on the main thread.
            views.pending_shared_destroys.push_back(removed->texture);
        }

        if (owned_world.is_valid())
            if (auto world_manager = services.world_manager.lock())
                world_manager->close_world(owned_world);
        if (auto rendering = services.rendering.lock())
            rendering->unregister_external_camera(removed->external_camera_id);
        refresh_present_callback(views, services);
        TBX_TRACE_INFO("StudioBridge: view stopped ('{}').", removed->name);
    }

    void stop_all_views(ViewState& views, const EngineServices& services)
    {
        std::vector<std::unique_ptr<ViewStream>> removed;
        auto owned_worlds = std::vector<tbx::Uuid>();
        {
            auto lock = std::lock_guard(views.mutex);
            if (views.streams.empty())
                return;

            removed = std::move(views.streams);
            views.streams.clear();
            views.inputs.clear();
            // Collect the view-owned (preview) worlds to release, then drop every registry reference. A
            // world.load'd world an editor view merely referenced isn't closed here — its Studio owner
            // releases it (world.close), and the WorldManager frees any stragglers at teardown.
            for (auto& view : removed)
                if (view->owns_world)
                    if (const auto world = views.worlds.find(view->world_id);
                        world != views.worlds.end() && world->second)
                        owned_worlds.push_back(world->second->id);
            views.worlds.clear();
            for (auto& view : removed)
                views.pending_shared_destroys.push_back(view->texture);
        }

        if (auto world_manager = services.world_manager.lock())
            for (const auto& id : owned_worlds)
                world_manager->close_world(id);
        if (auto rendering = services.rendering.lock())
            for (auto& view : removed)
                rendering->unregister_external_camera(view->external_camera_id);
        refresh_present_callback(views, services);
        TBX_TRACE_INFO("StudioBridge: all views stopped.");
    }

    void sync_game_cameras(ViewState& views, const EngineServices& services)
    {
        const auto active = services.active_world();

        auto lock = std::lock_guard(views.mutex);
        for (auto& view : views.streams)
        {
            auto* game = dynamic_cast<GameViewStream*>(view.get());
            if (game == nullptr)
                continue;

            // A game view mirrors ITS world's game camera — the active world for id 0, else its bound
            // world. Mirror the camera's pose + lens + tags so the view shows exactly what the player
            // sees, but keep our own render target.
            const auto world = game->world_id == 0U ? active : world_by_id_locked(views, game->world_id);
            if (!world)
                continue;
            auto game_camera = world->first_with<tbx::Camera>();
            if (!game_camera.get_id().is_valid())
                continue;
            const auto game_view = tbx::CameraView::from_entity(game_camera);
            if (!game_view.is_valid)
                continue;

            game->view = game_view;
            game->view.camera.set_target(game->texture);
        }
    }

    void push_external_cameras(ViewState& views, const EngineServices& services, bool is_playing)
    {
        auto rendering = services.rendering.lock();
        if (!rendering)
            return;

        // Frames a view keeps rendering at full rate after its last activity, so a just-finished drag or
        // a triggered async model load settles on screen before the engine throttles the view down.
        constexpr uint32 IDLE_SETTLE_FRAMES = 20U;

        auto lock = std::lock_guard(views.mutex);
        for (auto& view : views.streams)
        {
            // Each view renders the world at its world_id; id 0 (the active world) passes a null override
            // so the pipeline falls back to the active world. Re-resolved every frame so a bound world
            // that has since gone away (closed) cleanly reverts to nothing rather than a dangling render.
            auto world_override = world_by_id_locked(views, view->world_id);

            // Is anything happening in this view this frame? Play mode animates every view; a view still
            // creating/priming its surface must draw; focus, held buttons and pressed move-keys cover fly,
            // orbit and gizmo-drag interaction; an auto-orbiting preview animates on its own. Inspector
            // edits (no viewport input) aren't detected here — the engine's bounded idle refresh shows
            // those within a few frames, so a missed signal degrades to a small lag, never a frozen view.
            bool active = is_playing || view->surface_state != ViewSurfaceState::Presented;
            if (const auto input = views.inputs.find(view->name); input != views.inputs.end())
                active = active || input->second.focused || input->second.buttons != 0U
                         || input->second.move_keys != 0U || !input->second.keys.empty();
            if (const auto* preview = dynamic_cast<AssetPreviewViewStream*>(view.get()))
                active = active || preview->auto_orbit;

            if (active)
                view->idle_settle_frames = IDLE_SETTLE_FRAMES;
            else if (view->idle_settle_frames > 0U)
                --view->idle_settle_frames;

            auto camera = tbx::ExternalCamera {view->view, view->texture, std::move(world_override)};
            camera.render_active = active || view->idle_settle_frames > 0U;
            rendering->update_external_camera(view->external_camera_id, std::move(camera));
        }
    }

    void apply_view_input(ViewState& views, const tbx::Json& params)
    {
        const auto name = params.value(Wire::VIEW, std::string());
        if (name.empty())
            return;

        auto lock = std::lock_guard(views.mutex);
        // Only a live view (one with an input slot) accepts input; a late message for a stopped view is
        // dropped rather than leaking an orphan entry the consumers never read.
        const auto it = views.inputs.find(name);
        if (it == views.inputs.end())
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
        // Held keys as tbx::InputKey codes and the absolute mouse position within the view. Sent for
        // every view kind: the game input system consumes them while playing, editor views read them
        // for tool modifiers (the gizmo's snap-hold keys).
        input.keys.clear();
        if (const auto keys = params.find("keys"); keys != params.end() && keys->is_array())
            for (const auto& key : *keys)
                if (key.is_number_integer())
                    input.keys.push_back(key.get<int>());
        input.mouse_x = params.value("mouseX", 0.0F);
        input.mouse_y = params.value("mouseY", 0.0F);
    }

    Result frame_asset_preview(ViewState& views, const EngineServices& services, uint32 world_id)
    {
        auto assets = services.asset_manager.lock();
        if (!assets)
            return Result(false, "Asset manager is unavailable.");

        auto lock = std::lock_guard(views.mutex);
        for (auto& view : views.streams)
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

    Result resolve_view_camera(
        const ViewState& views, const std::string& view_name, tbx::CameraView& out_camera)
    {
        auto lock = std::lock_guard(views.mutex);
        for (const auto& view : views.streams)
        {
            if (view->name != view_name)
                continue;
            out_camera = view->view;
            return out_camera.is_valid ? Result::OK : Result(false, "View camera is unavailable.");
        }
        return Result(false, "Unknown view.");
    }

    std::shared_ptr<tbx::World> resolve_view_world(
        const ViewState& views, const EngineServices& services, const std::string& view_name)
    {
        auto world_id = 0U;
        auto found = false;
        {
            auto lock = std::lock_guard(views.mutex);
            for (const auto& view : views.streams)
                if (view->name == view_name)
                {
                    world_id = view->world_id;
                    found = true;
                    break;
                }
            if (found && world_id != 0U)
                return world_by_id_locked(views, world_id);
        }
        // The view renders the active world (id 0), or it is unknown — either way, the active world is
        // the right pick/draw target.
        return services.active_world();
    }

    std::shared_ptr<tbx::World> resolve_world_by_id(const ViewState& views, uint32 world_id)
    {
        auto lock = std::lock_guard(views.mutex);
        return world_by_id_locked(views, world_id);
    }

    uint32 register_loaded_world(ViewState& views, const std::shared_ptr<tbx::World>& world)
    {
        auto lock = std::lock_guard(views.mutex);
        const auto world_id = views.next_world_id++;
        views.worlds.emplace(world_id, world);
        return world_id;
    }

    tbx::Uuid unregister_world(ViewState& views, uint32 world_id)
    {
        if (world_id == 0U)
            return tbx::Uuid();

        auto lock = std::lock_guard(views.mutex);
        const auto it = views.worlds.find(world_id);
        if (it == views.worlds.end())
            return tbx::Uuid();

        const auto id = it->second ? it->second->id : tbx::Uuid();
        views.worlds.erase(it);
        return id;
    }

    std::shared_ptr<tbx::World> find_preview_world_with(const ViewState& views, const tbx::Uuid& id)
    {
        auto lock = std::lock_guard(views.mutex);
        for (const auto& entry : views.worlds)
            if (entry.second && entry.second->has(id))
                return entry.second;
        return nullptr;
    }
}
