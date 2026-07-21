#include "tbx/app.h"
#include "tbx/debug/log.h"
#include "tbx/debug/debug_view.h"
#include "tbx/gfx/gpu.h"
#include "tbx/audio/audio.h"
#include "tbx/ui/ui.h"
#include "tbx/ecs/block.h"
#include "tbx/physics/physics.h"
#include "tbx/platform/input.h"
#include "tbx/files/files.h"
#include "tbx/reflect/json_walker.h"
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
        RenderGraph render_graph = {};
        Scripts scripts; // constructed last, destroyed first — the VM dies before its world
        std::chrono::steady_clock::time_point previous_frame;
        std::optional<Json> pending_config; // a changed .tapp body awaiting re-apply
        std::unordered_set<Uuid> acquired_script_sources;
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
        register_block<Renderer>("Renderer")
            .field("material", &Renderer::material)
            .field("model", &Renderer::model);
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
        register_block<Ui>("Ui")
            .field("document", &Ui::document)
            .field("vertex", &Ui::vertex)
            .field("fragment", &Ui::fragment)
            .field("is_visible", &Ui::is_visible)
            .field("is_world_anchored", &Ui::is_world_anchored);
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

    /// @brief
    /// Purpose: Registers the App struct and its settings groups — the .tapp schema.
    static void register_app_types()
    {
        static bool g_registered = false;
        if (g_registered)
            return;
        g_registered = true;
        register_type<GraphicsSettings>("GraphicsSettings")
            .field("is_vsync_enabled", &GraphicsSettings::is_vsync_enabled)
            .field("shadow_resolution", &GraphicsSettings::shadow_resolution);
        register_type<PhysicsSettings>("PhysicsSettings")
            .field("fixed_timestep", &PhysicsSettings::fixed_timestep)
            .field("gravity", &PhysicsSettings::gravity);
        register_type<AudioSettings>("AudioSettings")
            .field("master_volume", &AudioSettings::master_volume);
        register_type<AssetSettings>("AssetSettings")
            .field("idle_lifetime_seconds", &AssetSettings::idle_lifetime_seconds);
        register_type<App>("App")
            .field("title", &App::title)
            .field("width", &App::width)
            .field("height", &App::height)
            .field("is_headless", &App::is_headless)
            .field("sandbox", &App::sandbox)
            .field("icon", &App::icon)
            .field("graphics", &App::graphics)
            .field("physics", &App::physics)
            .field("audio", &App::audio)
            .field("assets", &App::assets);
    }

    Result<App> load_app(const std::filesystem::path& tapp_file)
    {
        register_app_types();
        const auto text = files::read_text(tapp_file);
        if (!text)
            return std::unexpected(text.error());
        if (!is_valid(*text))
            return fail("'{}' is not a valid .tapp (JSON)", tapp_file.string());
        auto app = App {};
        if (auto read = json_read(get_type_registry().find("App")->get(), app, parse(*text));
            !read)
            return std::unexpected(read.error());
        app.asset_root = tapp_file.parent_path();
        app.config = AssetHandle<Json>(tapp_file.filename().string());
        return ok(std::move(app));
    }

    /// @brief
    /// Purpose: Pushes the App's settings into every subsystem — run at boot and again
    /// whenever the watched .tapp changes.
    static void apply_settings(App& app, AppState& state)
    {
        if (!state.window.is_headless())
        {
            state.window.set_title(app.title);
            state.window.set_vsync(app.graphics.is_vsync_enabled);
            if (app.icon.is_set())
            {
                if (const auto icon = state.assets.load_now(app.icon))
                    state.window.set_icon(
                        icon->get().width, icon->get().height, icon->get().pixels);
                else
                    TBX_WARN("window icon: {}", icon.error());
            }
        }
        set_shadow_resolution(app.graphics.shadow_resolution);
        physics::set_gravity(app.physics.gravity);
        audio::set_master_volume(app.audio.master_volume);
        state.assets.set_idle_lifetime(app.assets.idle_lifetime_seconds);
    }

    //// BOOT / SHUTDOWN ////

    static void boot(App& app)
    {
        register_builtin_blocks();
        register_app_types();
        g_state = std::make_unique<AppState>(app);
        AppState& state = *g_state;
        if (!state.window.is_headless())
        {
            gpu::initialize();
            gpu::set_viewport(state.window.get_width(), state.window.get_height());
        }
        if (!app.asset_root.empty())
            state.assets.set_root(app.asset_root);
        apply_settings(app, state);

        // The .tapp is an ordinary watched asset: loading it here registers it, and any
        // change queues its fresh body for re-apply on the next frame.
        if (app.config.is_set())
        {
            if (const auto config = state.assets.load_now(app.config))
                (void)config;
            else
                TBX_WARN("app config: {}", config.error());
            state.events.asset_reloaded.subscribe(
                &state,
                [&state](const AssetReloaded& reloaded)
                {
                    if (std::string_view(reloaded.extension) != ".tapp")
                        return;
                    if (const auto body =
                            state.assets.load_now(AssetHandle<Json>(reloaded.id)))
                        state.pending_config = body->get();
                });
        }

        // Idle-collected or hot-reloaded assets drop their render-side caches.
        state.events.asset_unloaded.subscribe(
            &state,
            [](const AssetUnloaded& unloaded) { forget_asset(unloaded.id); });

        // Changed .luau assets hot-reload their scripts; instances restart next update.
        state.events.asset_reloaded.subscribe(
            &state,
            [&state](const AssetReloaded& reloaded)
            {
                forget_asset(reloaded.id); // re-upload GPU copies of the fresh data
                if (!state.scripts.owns(reloaded.extension))
                    return; // not a script source — nothing to (re)register
                const auto script =
                    state.assets.load_now(AssetHandle<ScriptSource>(reloaded.id));
                if (script)
                {
                    if (const auto result = state.scripts.reload_source(
                            reloaded.id,
                            script->get().name,
                            script->get().source);
                        !result)
                        TBX_ERROR("{}", result.error());
                }
            });
        state.events.window_resized.subscribe(
            &state,
            [](const WindowResized& resized)
            { gpu::set_viewport(resized.width, resized.height); });

        // Configured content is ordinary assets: the sandbox layout opens through the kit
        // resolver, the UI document loads and shows. Failures request a clean exit.
        if (app.sandbox.is_set())
        {
            // Kit/level references inside the layout are Json assets too; streaming may call
            // the resolver from a worker, which the mutex-guarded asset maps support.
            const auto resolver = [&assets = state.assets](const std::string& reference)
                -> Result<Json>
            {
                auto body = assets.load_now(AssetHandle<Json>(reference));
                if (!body)
                    return std::unexpected(body.error());
                return ok(Json(body->get()));
            };
            const auto layout = state.assets.load_now(app.sandbox);
            if (!layout)
            {
                TBX_ERROR("sandbox '{}': {}", app.sandbox.path, layout.error());
                state.quit_requested = true;
            }
            else if (const auto opened = state.sandbox.open(
                         {.kits = layout->get(), .resolver = resolver});
                     !opened)
            {
                TBX_ERROR("sandbox '{}': {}", app.sandbox.path, opened.error());
                state.quit_requested = true;
            }
        }

        app.is_running = true;
        TBX_INFO(
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
            debug::reset();
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

        if (state.pending_config)
        {
            // The .tapp changed on disk: deserialize into the LIVE App and push settings out.
            if (const auto read = json_read(
                    get_type_registry().find("App")->get(), app, *state.pending_config);
                !read)
                TBX_ERROR("app config reload: {}", read.error());
            else
            {
                apply_settings(app, state);
                TBX_INFO("app settings re-applied from the .tapp");
            }
            state.pending_config.reset();
        }

        // The engine debug overlay rides F3.
        if (input::is_pressed(Key::F3))
            debug::toggle();
        debug::update(app);

        // Script sources referenced by spawned toys are ordinary assets: acquire each once —
        // the store emits asset_reloaded and the boot glue hands it to the right backend.
        for (auto&& [entity, script] : state.sandbox.get_registry().view<Script>().each())
        {
            if (script.source.id.is_nil()
                || state.acquired_script_sources.contains(script.source.id))
                continue;
            state.acquired_script_sources.insert(script.source.id);
            if (const auto acquired = state.assets.load_now(script.source); !acquired)
                TBX_ERROR(
                    "script source '{}': {}",
                    script.source.id.to_string(),
                    acquired.error());
        }

        state.assets.collect_garbage();
        state.scripts.update(app.delta_time);
        audio::update(state.sandbox, state.assets, app.delta_time);
        ui::update(app.delta_time);

        const float fixed_step = app.physics.fixed_timestep > 0.0f
            ? app.physics.fixed_timestep
            : 1.0f / 60.0f;
        static float g_fixed_accumulator = 0.0f;
        g_fixed_accumulator += app.delta_time;
        while (g_fixed_accumulator >= fixed_step)
        {
            g_fixed_accumulator -= fixed_step;
            state.scripts.fixed_update(fixed_step);
            physics::update(state.sandbox, state.assets, state.events, fixed_step);
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

    RenderGraph& get_render_graph()
    {
        return g_state->render_graph;
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
