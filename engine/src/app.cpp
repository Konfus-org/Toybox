#include "tbx/app.h"
#include "scripting/builtin_backends.h"
#include "tbx/audio/audio.h"
#include "tbx/audio/clip.h"
#include "tbx/debug/log.h"
#include "tbx/debug/debugging.h"
#include "tbx/ecs/billboard.h"
#include "tbx/gpu/camera.h"
#include "tbx/gpu/gpu.h"
#include "tbx/gpu/material.h"
#include "tbx/gpu/model.h"
#include "tbx/gpu/shader_source.h"
#include "tbx/gpu/texture.h"
#include "tbx/physics/physics.h"
#include "tbx/platform/input.h"
#include "tbx/reflection/reflection.h"
#include "tbx/reflection/type_registration.h"
#include "tbx/runtime.h"
#include "tbx/scripting/source.h"
#include "tbx/ui/document.h"
#include "tbx/ui/font.h"
#include "tbx/ui/ui.h"
#include "tbx/utils/cmdline_handler.h"
#include <chrono>
#include <cstddef>
#include <filesystem>
#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

namespace tbx
{
    /// @brief
    /// Purpose: Pushes the App's settings into every subsystem — run at boot and again
    /// whenever the watched .tapp changes.
    static void apply_settings(const App& app, RuntimeState& state)
    {
        if (!state.windows.open_windows.empty())
        {
            Window& window = state.windows.open_windows.front();
            window.title = app.config.title;
            if (app.config.icon.is_set())
            {
                if (const auto icon = load_asset_now(state.assets, state.events, app.config.icon))
                {
                    window.icon_width = icon->get().width;
                    window.icon_height = icon->get().height;
                    // A fresh vector on purpose: the backend detects a re-set icon by its
                    // data pointer changing (assign() could reuse the old buffer).
                    window.icon_pixels = std::vector<std::byte>(
                        icon->get().pixels.begin(),
                        icon->get().pixels.end());
                }
                else
                    TBX_WARN("window icon: {}", icon.error());
            }
        }
        // Settings are plain runtime state now: write the fields, the modules read them.
        // Vsync is the gpu module's request; the window backend applies it per surface.
        gpu_set_vsync(app.settings.graphics.is_vsync_enabled);
        state.renderer.shadow_resolution = app.settings.graphics.shadow_resolution;
        state.physics.gravity = app.settings.physics.gravity;
        state.audio.master_volume = app.settings.audio.master_volume;
        state.assets.idle_lifetime_seconds = app.settings.assets.idle_lifetime_seconds;
    }

    //// BOOT / SHUTDOWN ////

    static void boot(RuntimeState& state)
    {
        App& app = state.app;
        if (app.config.root_dir.empty())
        {
            TBX_ASSERT(
                false,
                "Root dir is not configured on App.config.root_dir, ensure this is set before "
                "launching a Toybox app!");
            app.status = AppStatus::QUIT_REQUESTED;
            return;
        }

        app.status = AppStatus::RUNNING;
        state.frame.previous = std::chrono::steady_clock::now();
        // Stands reflection + serializers up, sets the root, and discovers every asset.
        initialize_assets(state.assets, state.events, state.jobs, app.config.root_dir);
        apply_settings(app, state);

        // The engine ui font is an ordinary asset (resolved through the engine resources
        // root); games call set_font for their own faces.
        if (!app.config.root_dir.empty())
        {
            if (const auto font = load_asset_now(
                    state.assets,
                    state.events,
                    AssetHandle<Font>("Fonts/MontserratMedium.otf")))
                set_font(state.ui, font->get(), "Montserrat");
            else
                TBX_WARN("builtin ui font: {}", font.error());
        }

        // The app was already loaded before the runtime existed; scanning the app root
        // for its .tapp only registers the file with the watcher — no remembered path.
        auto ec = std::error_code();
        for (auto it = std::filesystem::directory_iterator(app.config.root_dir, ec);
             !ec && it != std::filesystem::directory_iterator();
             it.increment(ec))
        {
            const auto& entry = *it;
            if (entry.path().extension() != ".tapp")
                continue;
            if (const auto self = load_asset_now(
                    state.assets,
                    state.events,
                    AssetHandle<App>(entry.path().filename().generic_string()));
                !self)
                TBX_WARN("app config: {}", self.error());
            break; // the first .tapp is THE app file
        }

        // Subscribe to settings reload events
        state.events.asset_reloaded.subscribe(
            &state,
            [&state](const AssetReloaded& reloaded)
            {
                if (std::string_view(reloaded.extension.data()) != ".tapp")
                    return;
                const auto fresh =
                    load_asset_now(state.assets, state.events, AssetHandle<App>(reloaded.id));
                if (!fresh)
                {
                    TBX_ERROR("app config reload: {}", fresh.error());
                    return;
                }

                // resources is derived, never serialized — a fresh decode carries an
                // empty one, so the live value rides across the adoption.
                auto resources = std::move(state.app.config.root_dir);
                state.app.config = fresh->get().config;
                state.app.config.root_dir = std::move(resources);
                state.app.settings = fresh->get().settings;
                apply_settings(state.app, state);
                TBX_INFO("app settings re-applied from the .tapp");
            });

        // Idle-collected or hot-reloaded assets drop their render-side caches.
        state.events.asset_unloaded.subscribe(
            &state,
            [&state](const AssetUnloaded& unloaded)
            {
                gpu_forget_asset(state.renderer, unloaded.id);
            });

        // Changed .luau assets hot-reload their scripts; instances restart next update.
        state.events.asset_reloaded.subscribe(
            &state,
            [&state](const AssetReloaded& reloaded)
            {
                gpu_forget_asset(
                    state.renderer,
                    reloaded.id); // re-upload GPU copies of the fresh data
                if (!owns_script_extension(state.scripts, reloaded.extension.data()))
                    return; // not a script source — nothing to (re)register
                const auto script = load_asset_now(
                    state.assets,
                    state.events,
                    AssetHandle<ScriptSource>(reloaded.id));
                if (script)
                {
                    if (const auto result = reload_source(
                            state.scripts,
                            reloaded.id,
                            script->get().name,
                            script->get().source);
                        !result)
                        TBX_ERROR("{}", result.error());
                }
            });

        // Configured content is an ordinary asset: the level kit opens (its child kits
        // resolve through the sandbox's assets and spawn on the first stream() tick). Failure
        // requests a clean exit. Hosts without a configured level (selftest rigs, tools) drive
        // the sandbox by hand instead.
        if (app.config.sandbox.is_set())
        {
            const auto level = load_asset_now(state.assets, state.events, app.config.sandbox);
            if (!level)
            {
                TBX_ERROR("level '{}': {}", app.config.sandbox.path, level.error());
                app.status = AppStatus::QUIT_REQUESTED;
            }
            else
                state.sandbox.open(level->get());
        }

        initialize(state);

        if (state.windows.open_windows.empty())
            TBX_INFO("Toybox app up (headless)");
        else
            TBX_INFO(
                "Toybox app up ({}x{})",
                state.windows.open_windows.front().width,
                state.windows.open_windows.front().height);
    }

    //// THE LOOP ////

    /// @brief
    /// Purpose: One frustum per enabled camera in the scene — however many there are
    /// (splitscreen coop, editor viewports), each matched to its window exactly the way the
    /// renderer matches them (empty name = the main window, viewport rect scales the
    /// aspect), so streaming and rendering agree on what is in sight.
    static std::vector<Frustum> gather_camera_frustums(RuntimeState& state)
    {
        auto frustums = std::vector<Frustum>();
        if (state.windows.open_windows.empty())
            return frustums; // headless: no views, no streaming decisions
        state.sandbox.each<Camera>(
            [&](Toy toy, Camera& camera)
            {
                if (!toy.is_enabled())
                    return;
                const Window* window = nullptr;
                for (const Window& candidate : state.windows.open_windows)
                {
                    const bool is_main = &candidate == &state.windows.open_windows.front();
                    if (camera.window.empty() ? is_main : camera.window == candidate.name)
                    {
                        window = &candidate;
                        break;
                    }
                }
                if (!window || window->status != WindowStatus::OPEN)
                    return;
                const int width = static_cast<int>(camera.viewport.z * window->width);
                const int height = static_cast<int>(camera.viewport.w * window->height);
                if (width <= 0 || height <= 0)
                    return;
                frustums.push_back(gpu_make_frustum(
                    camera,
                    toy.get_world_transform(),
                    static_cast<float>(width) / height));
            });
        return frustums;
    }

    /// @brief
    /// Purpose: The world position of the first enabled camera, if any — what billboards
    /// turn to face.
    static std::optional<Vec3> primary_camera_position(RuntimeState& state)
    {
        auto position = std::optional<Vec3>();
        state.sandbox.each<Camera>(
            [&](Toy toy, Camera&)
            {
                if (position || !toy.is_enabled())
                    return;
                position = Vec3(toy.get_world_transform() * Vec4(0.0f, 0.0f, 0.0f, 1.0f));
            });
        return position;
    }

    bool run(Runtime& runtime)
    {
        auto& state = *runtime.state;

        App& app = state.app;
        if (app.status == AppStatus::STOPPED)
            return false;
        if (app.status == AppStatus::RUNNING)
            update_cmdline(state);
        if (app.status == AppStatus::CREATED)
            boot(state);

        // The windows present what was drawn since the last run() call, then pump OS events
        // (each window's first frame skips its present cleanly). The main window closing
        // stops the app; other windows just close.
        update_input(state.input);
        update_windows(state.windows, state.input, state.events);
        const bool window_alive =
            state.windows.open_windows.empty()
            || state.windows.open_windows.front().status == WindowStatus::OPEN;

        update_jobs(state.jobs);
        update_events(state.events);

        if (!window_alive || app.status == AppStatus::QUIT_REQUESTED)
        {
            // The loop is over; teardown is the Runtime destructor's business (reverse
            // declaration order: the module states first, then world, window, and app).
            app.status = AppStatus::STOPPED;
            return false;
        }

        const auto now = std::chrono::steady_clock::now();
        state.frame.delta_time = std::chrono::duration<float>(now - state.frame.previous).count();
        state.frame.previous = now;
        ++state.frame.index;

#ifdef TBX_DEBUGGING
        update_debugging(state);
#endif

        update_assets(state.assets, state.events);
        update_scripts(
            state.scripts,
            state.sandbox,
            state.assets,
            state.events,
            state.frame.delta_time);
        update_audio(
            state.audio,
            state.sandbox,
            state.assets,
            state.events,
            state.frame.delta_time);
        update_ui(state.ui, state.frame.delta_time);

        const float fixed_step = app.settings.physics.fixed_timestep > 0.0f
                                     ? app.settings.physics.fixed_timestep
                                     : 1.0f / 60.0f;
        state.frame.accumulator += state.frame.delta_time;
        while (state.frame.accumulator >= fixed_step)
        {
            state.frame.accumulator -= fixed_step;
            fixed_update_scripts(state.scripts, fixed_step);
            update_physics(state.physics, state.sandbox, state.assets, state.events, fixed_step);
        }

        // Billboards face the active camera (opt-in; nothing rotates without a Billboard
        // block) — done after scripts/physics settle transforms, before rendering.
        if (const auto camera_position = primary_camera_position(state))
            update_billboards(state.sandbox, *camera_position);

        // The engine pulls streaming: every enabled camera contributes a frustum (after
        // scripts/physics settled the transforms), and the sandbox loads what any of them
        // can see. Also flushes a pending open().
        const auto frustums = gather_camera_frustums(state);
        state.sandbox.stream(state.assets, state.events, state.jobs, frustums);

        // The engine renders by default; hosts with their own pipeline opt out and draw
        // between run() calls instead. Every open window gets a graph run; the first is
        // the main one (shadow map, post chain, UI).
        if (!app.settings.graphics.is_custom_pipeline)
        {
            for (Window& window : state.windows.open_windows)
            {
                if (window.status != WindowStatus::OPEN || !window.backend)
                    continue;
                make_current(window);
                gpu_set_viewport(window.width, window.height);
                auto context = RenderContext {
                    .renderer = state.renderer,
                    .sandbox = state.sandbox,
                    .assets = state.assets,
                    .events = state.events,
                    .ui = state.ui,
                    .debug = state.debug,
                    .window = window,
                    .is_main = &window == &state.windows.open_windows.front()};
                state.render_graph.render(context);
            }
        }

        return true;
    }

    void quit(Runtime& runtime)
    {
        auto& state = *runtime.state;

        if (state.app.status != AppStatus::STOPPED)
            state.app.status = AppStatus::QUIT_REQUESTED;
    }
}
