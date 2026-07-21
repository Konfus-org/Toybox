#include "tbx/app.h"
#include "tbx/audio/audio.h"
#include "tbx/debug/debug_view.h"
#include "tbx/debug/log.h"
#include "tbx/ecs/block.h"
#include "tbx/files/files.h"
#include "tbx/gfx/gpu.h"
#include "tbx/physics/physics.h"
#include "tbx/platform/input.h"
#include "tbx/reflect/json_walker.h"
#include "tbx/ui/ui.h"
#include <chrono>
#include <memory>
#include <unordered_set>

namespace tbx
{
    /// @brief
    /// Purpose: Everything the runtime owns, constructed in declaration order — that order IS
    /// the dependency graph, and reverse-order destruction IS shutdown.
    struct Runtime
    {
        Jobs jobs = {};
        Events events = {};
        Window window;
        Assets assets;
        Sandbox sandbox;
        RenderGraph render_graph = {};
        Scripts scripts; // constructed last, destroyed first — the VM dies before its world
        std::chrono::steady_clock::time_point previous_frame;
        std::optional<App> pending_app; // a changed .tapp, freshly decoded, awaiting re-apply
        std::unordered_set<Uuid> acquired_script_sources;
        bool quit_requested = false;

        explicit Runtime(const App& app)
            : window(
                  WindowDescription {
                      .title = app.config.title,
                      .width = app.config.width,
                      .height = app.config.height,
                      .is_headless = app.config.is_headless})
            , assets(jobs, events)
            , sandbox(jobs, assets)
            , scripts(sandbox, events)
            , previous_frame(std::chrono::steady_clock::now())
        {
        }
    };

    static std::unique_ptr<Runtime> g_state = {};

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
            .field("is_world_anchored", &Ui::is_world_anchored);
        register_block<Sky>("Sky").field("texture", &Sky::texture).field("tint", &Sky::tint);
        register_block<PostProcessing>("PostProcessing").field("shaders", &PostProcessing::shaders);
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
        register_type<AppConfig>("AppConfig")
            .field("title", &AppConfig::title)
            .field("width", &AppConfig::width)
            .field("height", &AppConfig::height)
            .field("is_headless", &AppConfig::is_headless)
            .field("sandbox", &AppConfig::sandbox)
            .field("icon", &AppConfig::icon);
        register_type<AppSettings>("AppSettings")
            .field("graphics", &AppSettings::graphics)
            .field("physics", &AppSettings::physics)
            .field("audio", &AppSettings::audio)
            .field("assets", &AppSettings::assets);
        register_type<App>("App")
            .field("config", &App::config)
            .field("settings", &App::settings);
    }

    template <>
    Result<App> load<App>(const std::filesystem::path& path)
    {
        register_app_types();
        const auto text = files::read_text(path);
        if (!text)
            return std::unexpected(text.error());
        if (!is_valid(*text))
            return fail("'{}' is not a valid .tapp (JSON)", path.string());
        auto app = App {};
        if (auto read = json_read(get_type_registry().find("App")->get(), app, parse(*text));
            !read)
            return std::unexpected(read.error());
        return ok(std::move(app));
    }

    Result<App> load_app(const std::filesystem::path& tapp_file)
    {
        auto app = load<App>(tapp_file);
        if (!app)
            return app;
        app->config.asset_root = tapp_file.parent_path();
        app->config.file = tapp_file.filename();
        return app;
    }

    /// @brief
    /// Purpose: Pushes the App's settings into every subsystem — run at boot and again
    /// whenever the watched .tapp changes.
    static void apply_settings(App& app, Runtime& state)
    {
        if (!state.window.is_headless())
        {
            state.window.set_title(app.config.title);
            state.window.set_vsync(app.settings.graphics.is_vsync_enabled);
            if (app.config.icon.is_set())
            {
                if (const auto icon = state.assets.load_now(app.config.icon))
                    state.window.set_icon(
                        icon->get().width,
                        icon->get().height,
                        icon->get().pixels);
                else
                    TBX_WARN("window icon: {}", icon.error());
            }
        }
        set_shadow_resolution(app.settings.graphics.shadow_resolution);
        physics::set_gravity(app.settings.physics.gravity);
        audio::set_master_volume(app.settings.audio.master_volume);
        state.assets.set_idle_lifetime(app.settings.assets.idle_lifetime_seconds);
    }

    //// BOOT / SHUTDOWN ////

    static void boot(App& app)
    {
        register_builtin_blocks();
        register_app_types();

        g_state = std::make_unique<Runtime>(app);
        Runtime& state = *g_state;

        if (!state.window.is_headless())
        {
            gpu::initialize();
            gpu::set_viewport(state.window.get_width(), state.window.get_height());
        }
        if (!app.config.asset_root.empty())
            state.assets.set_root(app.config.asset_root);

        apply_settings(app, state);

        // The .tapp is an ordinary watched asset and the App IS its asset type: loading it
        // here registers it, and any change queues a freshly decoded App for re-apply.
        if (!app.config.file.empty())
        {
            if (const auto self = state.assets.load_now(
                    AssetHandle<App>(app.config.file.generic_string()));
                !self)
                TBX_WARN("app config: {}", self.error());
            state.events.asset_reloaded.subscribe(
                &state,
                [&state](const AssetReloaded& reloaded)
                {
                    if (std::string_view(reloaded.extension) != ".tapp")
                        return;
                    if (const auto fresh = state.assets.load_now(AssetHandle<App>(reloaded.id)))
                        state.pending_app = fresh->get();
                    else
                        TBX_ERROR("app config reload: {}", fresh.error());
                });
        }

        // Idle-collected or hot-reloaded assets drop their render-side caches.
        state.events.asset_unloaded.subscribe(
            &state,
            [](const AssetUnloaded& unloaded)
            {
                forget_asset(unloaded.id);
            });

        // Changed .luau assets hot-reload their scripts; instances restart next update.
        state.events.asset_reloaded.subscribe(
            &state,
            [&state](const AssetReloaded& reloaded)
            {
                forget_asset(reloaded.id); // re-upload GPU copies of the fresh data
                if (!state.scripts.owns(reloaded.extension))
                    return; // not a script source — nothing to (re)register
                const auto script = state.assets.load_now(AssetHandle<ScriptSource>(reloaded.id));
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
            {
                gpu::set_viewport(resized.width, resized.height);
            });

        // Configured content is ordinary assets: the box opens (its kits resolve through
        // the sandbox's assets), the UI document loads and shows. Failures request a clean
        // exit.
        if (app.config.sandbox.is_set())
        {
            const auto box = state.assets.load_now(app.config.sandbox);
            if (!box)
            {
                TBX_ERROR("sandbox '{}': {}", app.config.sandbox.path, box.error());
                state.quit_requested = true;
            }
            else if (const auto opened = state.sandbox.open(box->get()); !opened)
            {
                TBX_ERROR("sandbox '{}': {}", app.config.sandbox.path, opened.error());
                state.quit_requested = true;
            }
        }

        app.state.is_running = true;
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
        Runtime& state = *g_state;

        // Present what the host drew since the last run() call (skips cleanly on frame 0).
        if (app.state.frame > 0)
            state.window.swap();

        input::pump();

        const bool window_alive = state.window.pump(state.events);

        state.jobs.drain_main();
        state.events.drain();

        if (!window_alive || state.quit_requested)
        {
            app.state.is_running = false;
            debug::reset();
            ui::reset();
            audio::reset();
            physics::reset();
            g_state.reset(); // reverse-declaration-order shutdown
            return false;
        }

        const auto now = std::chrono::steady_clock::now();
        app.state.delta_time = std::chrono::duration<float>(now - state.previous_frame).count();
        state.previous_frame = now;
        ++app.state.frame;

        if (state.pending_app)
        {
            // The .tapp changed on disk: adopt its config + settings (state is runtime-only,
            // and the derived paths cannot change while running) and push settings out.
            state.pending_app->config.asset_root = app.config.asset_root;
            state.pending_app->config.file = app.config.file;
            app.config = state.pending_app->config;
            app.settings = state.pending_app->settings;
            state.pending_app.reset();
            apply_settings(app, state);
            TBX_INFO("app settings re-applied from the .tapp");
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
                TBX_ERROR("script source '{}': {}", script.source.id.to_string(), acquired.error());
        }

        state.assets.collect_garbage();
        state.scripts.update(app.state.delta_time);
        audio::update(state.sandbox, state.assets, app.state.delta_time);
        ui::update(app.state.delta_time);

        const float fixed_step =
            app.settings.physics.fixed_timestep > 0.0f ? app.settings.physics.fixed_timestep : 1.0f / 60.0f;
        static float g_fixed_accumulator = 0.0f;
        g_fixed_accumulator += app.state.delta_time;
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
