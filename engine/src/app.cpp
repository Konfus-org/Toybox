#include "tbx/app.h"
#include "tbx/core/log.h"
#include "tbx/gfx/gpu.h"
#include "tbx/gfx/render_blocks.h"
#include "tbx/physics/physics.h"
#include "tbx/platform/input.h"
#include <chrono>
#include <memory>

namespace tbx
{
    /// @brief
    /// Purpose: Everything the runtime owns, constructed in declaration order — that order IS
    /// the dependency graph, and reverse-order destruction IS shutdown.
    struct AppState
    {
        Jobs jobs = {};
        Events events = {};
        Window window;
        Assets assets;
        Sandbox sandbox;
        Scripts scripts; // constructed last, destroyed first — the VM dies before its world
        std::chrono::steady_clock::time_point previous_frame;
        bool quit_requested = false;

        explicit AppState(const App& app)
            : window(
                  WindowDescription {
                      .title = app.title,
                      .width = app.width,
                      .height = app.height,
                      .is_headless = app.is_headless})
            , assets(jobs, events)
            , sandbox(jobs)
            , scripts(sandbox, events)
            , previous_frame(std::chrono::steady_clock::now())
        {
        }
    };

    static std::unique_ptr<AppState> g_state = {};

    //// BOOT / SHUTDOWN ////

    static void boot(App& app)
    {
        gpu::register_render_blocks();
        physics::register_physics_blocks();
        g_state = std::make_unique<AppState>(app);
        AppState& state = *g_state;
        if (!state.window.is_headless())
        {
            gpu::initialize();
            gpu::set_viewport(state.window.get_width(), state.window.get_height());
        }
        if (!app.asset_root.empty())
            state.assets.set_root(app.asset_root);

        // Changed .luau assets hot-reload their scripts; instances restart next update.
        state.events.asset_reloaded.subscribe(
            &state,
            [&state](const AssetReloaded& reloaded)
            {
                if (const auto script = state.assets.get_script(reloaded.id))
                {
                    if (const auto result =
                            state.scripts.reload_source(script->name, script->source);
                        !result)
                        log_error("{}", result.error());
                }
            });
        state.events.window_resized.subscribe(
            &state,
            [](const WindowResized& resized)
            { gpu::set_viewport(resized.width, resized.height); });

        app.is_running = true;
        log_info(
            "Toybox app up ({}x{}{})",
            state.window.get_width(),
            state.window.get_height(),
            state.window.is_headless() ? ", headless" : "");
    }

    //// THE LOOP ////

    bool run(App& app)
    {
        if (!g_state)
            boot(app);
        AppState& state = *g_state;

        // Present what the host drew since the last run() call (skips cleanly on frame 0).
        if (app.frame > 0)
            state.window.swap();

        input::pump();
        const bool window_alive = state.window.pump(state.events);
        state.jobs.drain_main();
        state.events.drain();

        if (!window_alive || state.quit_requested)
        {
            app.is_running = false;
            physics::reset();
            g_state.reset(); // reverse-declaration-order shutdown
            return false;
        }

        const auto now = std::chrono::steady_clock::now();
        app.delta_time = std::chrono::duration<float>(now - state.previous_frame).count();
        state.previous_frame = now;
        ++app.frame;

        state.scripts.update(app.delta_time);

        static constexpr float FIXED_STEP = 1.0f / 60.0f;
        static float g_fixed_accumulator = 0.0f;
        g_fixed_accumulator += app.delta_time;
        while (g_fixed_accumulator >= FIXED_STEP)
        {
            g_fixed_accumulator -= FIXED_STEP;
            state.scripts.fixed_update(FIXED_STEP);
            physics::step(state.sandbox, state.events, FIXED_STEP);
        }
        return true;
    }

    void quit()
    {
        if (g_state)
            g_state->quit_requested = true;
    }

    //// SUBSYSTEM ACCESS ////

    Assets& get_assets()
    {
        return g_state->assets;
    }

    Events& get_events()
    {
        return g_state->events;
    }

    Jobs& get_jobs()
    {
        return g_state->jobs;
    }

    Sandbox& get_sandbox()
    {
        return g_state->sandbox;
    }

    Scripts& get_scripts()
    {
        return g_state->scripts;
    }

    Window& get_window()
    {
        return g_state->window;
    }
}
