#include "tbx/app.h"
#include "tbx/core/log.h"
#include "tbx/gfx/gpu.h"
#include "tbx/audio/audio.h"
#include "tbx/ui/ui.h"
#include "tbx/ecs/block.h"
#include "tbx/physics/physics.h"
#include "tbx/platform/input.h"
#include <chrono>
#include <memory>
#include <unordered_set>

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
        std::unordered_set<Uuid> acquired_script_sources;
        uint64 ui_document = 0;
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

    //// REGISTRATION ////

    void register_builtin_blocks()
    {
        static bool g_registered = false;
        if (g_registered)
            return;
        g_registered = true;
        register_block<Transform>("Transform")
            .field("position", &Transform::position)
            .field("rotation", &Transform::rotation)
            .field("scale", &Transform::scale);
        register_block<Camera>("Camera")
            .field("fov_degrees", &Camera::fov_degrees)
            .field("near_plane", &Camera::near_plane)
            .field("far_plane", &Camera::far_plane);
        register_block<MeshRenderer>("MeshRenderer")
            .field("material", &MeshRenderer::material)
            .field("model", &MeshRenderer::model)
            .field("texture", &MeshRenderer::texture)
            .field("mesh", &MeshRenderer::mesh)
            .field("tint", &MeshRenderer::tint);
        register_block<DirectionalLight>("DirectionalLight")
            .field("color", &DirectionalLight::color)
            .field("intensity", &DirectionalLight::intensity);
        register_block<RigidBody>("RigidBody")
            .field("mass", &RigidBody::mass)
            .field("is_kinematic", &RigidBody::is_kinematic);
        register_block<Collider>("Collider")
            .field("shape", &Collider::shape)
            .field("half_extents", &Collider::half_extents)
            .field("radius", &Collider::radius)
            .field("height", &Collider::height);
        register_block<Sky>("Sky")
            .field("texture", &Sky::texture)
            .field("tint", &Sky::tint);
        register_block<PostProcessing>("PostProcessing")
            .field("shaders", &PostProcessing::shaders);
        register_block<Script>("Script").field("source", &Script::source);
        register_block<AudioListener>("AudioListener").field("volume", &AudioListener::volume);
        register_block<AudioSource>("AudioSource")
            .field("clip", &AudioSource::clip)
            .field("volume", &AudioSource::volume)
            .field("is_looping", &AudioSource::is_looping)
            .field("is_playing", &AudioSource::is_playing);
    }

    //// BOOT / SHUTDOWN ////

    static void boot(App& app)
    {
        register_builtin_blocks();
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
                const auto script =
                    state.assets.get(AssetHandle<ScriptSource> {.id = reloaded.id});
                if (script)
                {
                    if (const auto result = state.scripts.reload_source(
                            reloaded.id,
                            script->get().name,
                            script->get().source);
                        !result)
                        log_error("{}", result.error());
                }
            });
        state.events.window_resized.subscribe(
            &state,
            [](const WindowResized& resized)
            { gpu::set_viewport(resized.width, resized.height); });

        // Configured content is ordinary assets: the sandbox layout opens through the kit
        // resolver, the UI document loads and shows. Failures request a clean exit.
        if (!app.sandbox.empty())
        {
            const auto layout = state.assets.load_now<Json>(app.sandbox);
            const auto body = layout ? state.assets.get(*layout) : std::nullopt;
            if (!body)
            {
                log_error(
                    "sandbox '{}': {}",
                    app.sandbox,
                    layout ? "did not load" : layout.error());
                state.quit_requested = true;
            }
            else if (const auto opened = state.sandbox.open(
                         {.kits = body->get(), .resolver = state.assets.make_kit_resolver()});
                     !opened)
            {
                log_error("sandbox '{}': {}", app.sandbox, opened.error());
                state.quit_requested = true;
            }
        }
        if (!app.ui.empty() && !state.window.is_headless())
        {
            const auto document = state.assets.load_now<UiDocument>(app.ui);
            const auto body = document ? state.assets.get(*document) : std::nullopt;
            if (!body)
                log_error(
                    "ui '{}': {}",
                    app.ui,
                    document ? std::string("did not load") : document.error());
            else if (const auto shown = ui::load_document(body->get().text))
                state.ui_document = *shown;
            else
                log_error("ui '{}': {}", app.ui, shown.error());
        }

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
            ui::reset();
            audio::reset();
            physics::reset();
            g_state.reset(); // reverse-declaration-order shutdown
            return false;
        }

        const auto now = std::chrono::steady_clock::now();
        app.delta_time = std::chrono::duration<float>(now - state.previous_frame).count();
        state.previous_frame = now;
        ++app.frame;

        // Script sources referenced by spawned toys are ordinary assets: acquire each once —
        // the store emits asset_reloaded and the boot glue hands it to the right backend.
        for (auto&& [entity, script] : state.sandbox.get_registry().view<Script>().each())
        {
            if (script.source.id.is_nil()
                || state.acquired_script_sources.contains(script.source.id))
                continue;
            state.acquired_script_sources.insert(script.source.id);
            if (const auto acquired = state.assets.acquire(script.source); !acquired)
                log_error(
                    "script source '{}': {}",
                    script.source.id.to_string(),
                    acquired.error());
        }

        state.scripts.update(app.delta_time);
        audio::update(state.sandbox, state.assets, app.delta_time);
        ui::update(app.delta_time);

        static constexpr float FIXED_STEP = 1.0f / 60.0f;
        static float g_fixed_accumulator = 0.0f;
        g_fixed_accumulator += app.delta_time;
        while (g_fixed_accumulator >= FIXED_STEP)
        {
            g_fixed_accumulator -= FIXED_STEP;
            state.scripts.fixed_update(FIXED_STEP);
            physics::update(state.sandbox, state.events, FIXED_STEP);
        }
        return true;
    }

    void quit()
    {
        if (g_state)
            g_state->quit_requested = true;
    }

    bool is_app_running()
    {
        return g_state != nullptr;
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

    uint64 get_ui_document()
    {
        return g_state ? g_state->ui_document : 0;
    }

    Window& get_window()
    {
        return g_state->window;
    }
}
