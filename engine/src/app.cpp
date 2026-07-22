#include "tbx/app.h"
#include "scripting/builtin_backends.h"
#include "tbx/audio/audio.h"
#include "tbx/audio/audio_clip.h"
#include "tbx/cmdline_handler.h"
#include "tbx/debug/debug_view.h"
#include "tbx/debug/log.h"
#include "tbx/gfx/gpu.h"
#include "tbx/gfx/material.h"
#include "tbx/gfx/model.h"
#include "tbx/gfx/shader_source.h"
#include "tbx/gfx/texture.h"
#include "tbx/physics/physics.h"
#include "tbx/platform/input.h"
#include "tbx/reflection/reflection.h"
#include "tbx/reflection/type_registration.h"
#include "tbx/runtime.h"
#include "tbx/scripting/script_source.h"
#include "tbx/ui/font.h"
#include "tbx/ui/ui.h"
#include "tbx/ui/ui_document.h"
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
        if (!state.windows.windows.empty())
        {
            windows::Window& window = state.windows.windows.front();
            window.title = app.config.title;
            window.is_vsync_enabled = app.settings.graphics.is_vsync_enabled;
            if (app.config.icon.is_set())
            {
                if (const auto icon = assets::load_now(state.assets, state.events, app.config.icon))
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

        reflection::initialize();
        app.status = AppStatus::RUNNING;
        state.frame.previous = std::chrono::steady_clock::now();
        assets::set_root(state.assets, state.events, state.jobs, app.config.root_dir);
        apply_settings(app, state);

        // The engine ui font is an ordinary asset (resolved through the engine resources
        // root); games call ui::set_font for their own faces.
        if (!app.config.root_dir.empty())
        {
            if (const auto font = assets::load_now(
                    state.assets,
                    state.events,
                    AssetHandle<Font>("Fonts/MontserratMedium.otf")))
                ui::set_font(state.ui, font->get(), "Montserrat");
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
            if (const auto self = assets::load_now(
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
            [&state](const events::AssetReloaded& reloaded)
            {
                if (std::string_view(reloaded.extension.data()) != ".tapp")
                    return;
                const auto fresh =
                    assets::load_now(state.assets, state.events, AssetHandle<App>(reloaded.id));
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
            [&state](const events::AssetUnloaded& unloaded)
            {
                forget_asset(state.renderer, unloaded.id);
            });

        // Changed .luau assets hot-reload their scripts; instances restart next update.
        state.events.asset_reloaded.subscribe(
            &state,
            [&state](const events::AssetReloaded& reloaded)
            {
                forget_asset(state.renderer, reloaded.id); // re-upload GPU copies of the fresh data
                if (!scripts::owns(state.scripts, reloaded.extension.data()))
                    return; // not a script source — nothing to (re)register
                const auto script = assets::load_now(
                    state.assets,
                    state.events,
                    AssetHandle<ScriptSource>(reloaded.id));
                if (script)
                {
                    if (const auto result = scripts::reload_source(
                            state.scripts,
                            reloaded.id,
                            script->get().name,
                            script->get().source);
                        !result)
                        TBX_ERROR("{}", result.error());
                }
            });

        // Configured content is ordinary assets: the box opens (its kits resolve through
        // the sandbox's assets), the UI document loads and shows. Failures request a clean
        // exit. Hosts without a configured box (selftest rigs, tools) drive the sandbox by
        // hand instead.
        if (app.config.sandbox.is_set())
        {
            const auto box = assets::load_now(state.assets, state.events, app.config.sandbox);
            if (!box)
            {
                TBX_ERROR("sandbox '{}': {}", app.config.sandbox.path, box.error());
                app.status = AppStatus::QUIT_REQUESTED;
            }
            else if (
                const auto opened = state.sandbox.open(state.assets, state.events, box->get());
                !opened)
            {
                TBX_ERROR("sandbox '{}': {}", app.config.sandbox.path, opened.error());
                app.status = AppStatus::QUIT_REQUESTED;
            }
        }

        scripts::initialize(state);

        if (state.windows.windows.empty())
            TBX_INFO("Toybox app up (headless)");
        else
            TBX_INFO(
                "Toybox app up ({}x{})",
                state.windows.windows.front().width,
                state.windows.windows.front().height);
    }

    //// THE LOOP ////

    bool run(Runtime& runtime)
    {
        auto& state = *runtime.state;

        App& app = state.app;
        if (app.status == AppStatus::STOPPED)
            return false;
        if (app.status == AppStatus::RUNNING)
            cmdline::update(state);
        if (app.status == AppStatus::CREATED)
            boot(state);

        // The windows present what was drawn since the last run() call, then pump OS events
        // (each window's first frame skips its present cleanly). The main window closing
        // stops the app; other windows just close.
        input::update(state.input);
        windows::update(state.windows, state.input, state.events);
        const bool window_alive =
            state.windows.windows.empty()
            || state.windows.windows.front().status == windows::WindowStatus::OPEN;

        jobs::update(state.jobs);
        events::update(state.events);

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

        debug::view::update(
            state.debug,
            state.input,
            state.sandbox,
            state.assets,
            state.windows,
            state.ui,
            state.frame.delta_time);
        assets::update(state.assets, state.events);
        scripts::update(
            state.scripts,
            state.sandbox,
            state.assets,
            state.events,
            state.frame.delta_time);
        audio::update(
            state.audio,
            state.sandbox,
            state.assets,
            state.events,
            state.frame.delta_time);
        ui::update(state.ui, state.frame.delta_time);

        const float fixed_step = app.settings.physics.fixed_timestep > 0.0f
                                     ? app.settings.physics.fixed_timestep
                                     : 1.0f / 60.0f;
        state.frame.accumulator += state.frame.delta_time;
        while (state.frame.accumulator >= fixed_step)
        {
            state.frame.accumulator -= fixed_step;
            scripts::fixed_update(state.scripts, fixed_step);
            physics::update(state.physics, state.sandbox, state.assets, state.events, fixed_step);
        }

        // The engine renders by default; hosts with their own pipeline opt out and draw
        // between run() calls instead. Every open window gets a graph run; the first is
        // the main one (shadow map, post chain, UI).
        if (!app.settings.graphics.is_custom_pipeline)
        {
            for (windows::Window& window : state.windows.windows)
            {
                if (window.status != windows::WindowStatus::OPEN || !window.backend)
                    continue;
                windows::make_current(window);
                gpu::set_viewport(window.width, window.height);
                auto context = RenderContext {
                    .renderer = state.renderer,
                    .sandbox = state.sandbox,
                    .assets = state.assets,
                    .events = state.events,
                    .ui = state.ui,
                    .debug = state.debug,
                    .window = window,
                    .is_main = &window == &state.windows.windows.front()};
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
