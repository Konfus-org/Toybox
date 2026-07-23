#include "tbx/app.h"
#include "tbx/audio/audio.h"
#include "tbx/debug/debugging.h"
#include "tbx/debug/log.h"
#include "tbx/gfx/gpu.h"
#include "tbx/gfx/texture.h"
#include "tbx/physics/physics.h"
#include "tbx/platform/input.h"
#include "tbx/runtime.h"
#include "tbx/scripting/source.h"
#include "tbx/ui/font.h"
#include "tbx/ui/ui.h"
#include "tbx/utils/cmdline_handler.h"
#include <chrono>
#include <cstddef>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace tbx
{
    // ---- Internal (engine orchestration; not the user-facing API) ----
    namespace internal
    {
        // The one published runtime (see runtime.h current()). Set by run() for the running frame,
        // cleared on stop. Main-thread only; the script-facing free-function API reads it.
        static RuntimeState* g_current = nullptr;
    
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
            set_vsync(app.settings.graphics.is_vsync_enabled);
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
            // Wire the sandbox to the asset system so kit add/instantiation reaches it directly.
            state.sandbox.assets = &state.assets;
            state.sandbox.events = &state.events;
            // Stands reflection + serializers up, sets the root, and discovers every asset.
            initialize_assets(state.assets, state.events, state.jobs, app.config.root_dir);
            apply_settings(app, state);
            // The render graph lives on the render state (plain data). Seed it with the standard
            // passes (games may replace state.renderer.render_graph.passes to author their own).
            state.renderer.render_graph = make_default_render_graph();
    
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
            state.events.signal<AssetReloaded>().subscribe(
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
    
            // Script sources are assets: compile on first load, recompile on change. Both paths run
            // the same (re)registration — reload_script also serves as the initial load — so a script
            // just listens for its own asset type and ignores everything else.
            const auto register_script_asset =
                [&state](const Uuid& id, const std::string_view extension)
            {
                bool is_script_source = false;
                for (const auto& backend : state.scripts.backends)
                    if (backend->owns_extension(extension))
                        is_script_source = true;
                if (!is_script_source)
                    return; // not a script source — nothing to (re)register
    
                const auto script =
                    load_asset_now(state.assets, state.events, AssetHandle<ScriptSource>(id));
                if (!script)
                    return;
    
                // The script is plain text: `text` is the source; its file name (from the asset's
                // path) is the diagnostics name for traces.
                const std::string name = std::filesystem::path(script->get().path).filename().string();
                if (const auto result = compile_script(state.scripts, id, name, script->get().text);
                    !result)
                    TBX_ERROR("{}", result.error());
            };
            state.events.signal<AssetLoaded>().subscribe(
                &state,
                [register_script_asset](const AssetLoaded& loaded)
                {
                    register_script_asset(loaded.id, loaded.extension.data());
                });
    
            // Idle-collected or hot-reloaded assets drop their render-side caches.
            state.events.signal<AssetUnloaded>().subscribe(
                &state,
                [&state](const AssetUnloaded& unloaded)
                {
                    gpu_purge(state.renderer, unloaded.id);
                });
    
            // Changed assets re-upload their GPU copies; changed .luau assets recompile and restart.
            state.events.signal<AssetReloaded>().subscribe(
                &state,
                [&state, register_script_asset](const AssetReloaded& reloaded)
                {
                    gpu_purge(state.renderer,
                              reloaded.id); // re-upload GPU copies of the fresh data
                    register_script_asset(reloaded.id, reloaded.extension.data());
                });
    
            // Configured content is an ordinary asset: the level kit opens (its child kits
            // resolve through the sandbox's assets and add on the first stream() tick). Failure
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
                    open(state.sandbox, level->get());
            }
    
            initialize_scripting(state);
    
            if (state.windows.open_windows.empty())
                TBX_INFO("Toybox app up (headless)");
            else
                TBX_INFO(
                    "Toybox app up ({}x{})",
                    state.windows.open_windows.front().width,
                    state.windows.open_windows.front().height);
        }
    }

    //// THE LOOP ////

    bool run(Runtime& runtime)
    {
        auto& state = *runtime.state;

        App& app = state.app;
        if (app.status == AppStatus::STOPPED)
            return false;

        // Publish the runtime for this frame so the free-function API (tbx::current) resolves
        // during scripts/systems. Stable heap address, so it survives Runtime handle moves between
        // frames.
        internal::g_current = &state;
        if (app.status == AppStatus::RUNNING)
            internal::update_cmdline(state);
        if (app.status == AppStatus::CREATED)
            internal::boot(state);

        // The windows present what was drawn since the last run() call, then drain the window
        // events they own (each window's first frame skips its present cleanly). Windows run
        // first so SDL is initialized before update_input pumps — it rolls the input frame and
        // then drains the remaining keyboard/mouse/controller events. The main window closing
        // stops the app; other windows just close.
        internal::update_windows(state.windows, state.input, state.events);
        internal::update_input(state.input, state.events);
        internal::update_jobs(state.jobs);
        internal::update_events(state.events);

        const bool window_alive =
            state.windows.open_windows.empty()
            || state.windows.open_windows.front().status == WindowStatus::OPEN;
        if (!window_alive || app.status == AppStatus::QUIT_REQUESTED)
        {
            // The loop is over. Fire each script's cleanup and free it while the sandbox and VMs
            // are both still alive — the rest of teardown is the Runtime destructor's business
            // (reverse declaration order: the module states first, then world, window, and app).
            internal::purge_scripts(state.scripts, state.sandbox);
            app.status = AppStatus::STOPPED;
            internal::g_current = nullptr; // the state is about to be torn down by the Runtime destructor
            return false;
        }

        const auto now = std::chrono::steady_clock::now();
        state.frame.delta_time = std::chrono::duration<float>(now - state.frame.previous).count();
        state.frame.previous = now;
        ++state.frame.index;

#ifdef TBX_DEBUGGING
        internal::update_debugging(state);
#endif

        internal::update_assets(state.assets, state.events);
        internal::update_scripts(
            state.scripts,
            state.sandbox,
            state.assets,
            state.events,
            state.frame.delta_time);
        internal::update_audio(
            state.audio,
            state.sandbox,
            state.assets,
            state.events,
            state.frame.delta_time);
        internal::update_ui(state.ui, state.frame.delta_time);

        const float fixed_step = app.settings.physics.fixed_timestep > 0.0f
                                     ? app.settings.physics.fixed_timestep
                                     : 1.0f / 60.0f;
        state.frame.accumulator += state.frame.delta_time;
        while (state.frame.accumulator >= fixed_step)
        {
            state.frame.accumulator -= fixed_step;
            internal::fixed_update_scripts(state.scripts, state.sandbox, fixed_step);
            internal::update_physics(state.physics, state.sandbox, state.assets, state.events, fixed_step);
        }

        // The per-frame ECS tick: builtin components (billboards) face the active camera, then
        // streaming loads what any enabled camera can see (also flushes a pending open()). Every
        // enabled camera contributes a frustum, gathered here after scripts/physics settled the
        // transforms and before rendering.
        internal::update_sandbox(state.sandbox, state.assets, state.events, state.jobs, state.windows);

        // The engine renders by default; hosts with their own pipeline opt out and draw
        // between run() calls instead. Every open window gets a graph run; the first is
        // the main one (shadow map, post chain, UI).
        if (!app.settings.graphics.is_custom_pipeline)
        {
            for (Window& window : state.windows.open_windows)
            {
                if (window.status != WindowStatus::OPEN || !window.backend)
                    continue;
                auto context = RenderContext {
                    .renderer = state.renderer,
                    .sandbox = state.sandbox,
                    .assets = state.assets,
                    .events = state.events,
                    .ui = state.ui,
                    .debug = state.debug,
                    .window = window,
                    .is_main = &window == &state.windows.open_windows.front(),
                };
                render(context);
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

    RuntimeState& current()
    {
        TBX_ASSERT(internal::g_current != nullptr, "tbx::current() called with no running runtime");
        return *internal::g_current;
    }

    bool has_current()
    {
        return internal::g_current != nullptr;
    }

    void quit()
    {
        if (has_current() && current().app.status != AppStatus::STOPPED)
            current().app.status = AppStatus::QUIT_REQUESTED;
    }
}
