#pragma once
#include "tbx/interfaces/file_ops.h"
#include "tbx/interfaces/window_manager.h"
#include "tbx/systems/input/input_manager.h"
#include "tbx/systems/app/app_service_provider.h"
#include "tbx/systems/app/command_list.h"
#include "tbx/systems/app/settings.h"
#include "tbx/systems/assets/manager.h"
#include "tbx/systems/async/thread_manager.h"
#include "tbx/systems/world/manager.h"
#include "tbx/systems/graphics/gizmos.h"
#include "tbx/systems/graphics/rendering.h"
#include "tbx/systems/physics/physics.h"
#include "tbx/systems/plugin_api/plugin_manager.h"
#include "tbx/systems/plugin_api/service_provider.h"
#include "tbx/systems/scripting/script_system.h"
#include "tbx/systems/time/delta_time.h"
#include "tbx/tbx_api.h"
#include <atomic>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <thread>

#if defined(TBX_PLATFORM_WINDOWS)
    #define TBX_APP_ENTRY_EXPORT extern "C" __declspec(dllexport)
#else
    #define TBX_APP_ENTRY_EXPORT extern "C"
#endif

namespace tbx
{
    class TBX_API Application
    {
      public:
        Application();
        virtual ~Application() noexcept;

      public:
        Application(const Application&) = delete;
        Application& operator=(const Application&) = delete;
        Application(Application&&) = delete;
        Application& operator=(Application&&) = delete;

      public:
        int run(const CommandList& command_list, const std::filesystem::path& root_directory);
        ServiceProvider& get_service_provider();
        const ServiceProvider& get_service_provider() const;
        PluginManager& get_plugin_manager();
        const PluginManager& get_plugin_manager() const;
        const AppSettings& get_settings() const;
        const std::string& get_name() const;
        const Window& get_main_window() const;
        bool should_exit() const;
        void request_exit();
        bool is_paused() const;
        void set_paused(bool is_paused);

      protected:
        virtual int initialize(
            const CommandList& command_list,
            const std::filesystem::path& root_directory);
        virtual void shutdown();
        virtual void update(const DeltaTime& delta_time);
        virtual void fixed_update(const DeltaTime& delta_time);

      private:
        // Startup phases, called in order by initialize(). Each owns one //// INITIALIZE //// stage
        // so the top-level flow reads as a sequence of named steps instead of one long body.
        void build_core_services(
            const std::filesystem::path& root_directory,
            std::vector<std::filesystem::path> startup_asset_directories);
        void load_plugins(const CommandList& command_list);
        int register_runtime_services(const CommandList& command_list);
        void register_message_handlers(const Handle& startup_settings_handle);
        int open_main_window(const CommandList& command_list);
        void log_startup_environment(const CommandList& command_list) const;
        void start_parent_watchdog(const CommandList& command_list);

        std::shared_ptr<AppSettings> load_app_settings(const Handle& settings_handle);
        std::vector<std::string> resolve_plugins(
            const std::vector<std::string>& settings_plugins,
            const std::vector<std::string>& command_plugins);
        // Re-applies the AppSettings fields that are consumed once (not per-frame) to their subsystems after a
        // live settings reload — currently the window title (name) and render resolution (window size).
        // Per-frame-read fields (world streaming, vsync, shadows, physics) are picked up automatically.
        void apply_runtime_settings(const AppSettings& previous, const AppSettings& current);

      private:
        std::atomic<bool> _should_exit = false;
        bool _is_headless = false;
        bool _is_hidden = false;
        bool _hidden_context_primed = false;
        bool _is_paused = false;
        std::string _name = "Toybox App";

        AppServiceProvider _services = {};
        std::unique_ptr<PluginManager> _plugin_manager = {};
        std::weak_ptr<IFileOps> _file_ops = {};
        std::weak_ptr<IMessageCoordinator> _msg_coordinator = {};
        std::shared_ptr<AppSettings> _settings = {};
        std::weak_ptr<AssetManager> _asset_manager = {};
        std::weak_ptr<WorldManager> _world_manager = {};
        std::weak_ptr<ThreadManager> _thread_manager = {};
        std::weak_ptr<IWindowManager> _window_manager = {};
        std::weak_ptr<InputManager> _input_manager = {};
        std::weak_ptr<Physics> _physics = {};
        std::weak_ptr<Gizmos> _gizmos = {};
        std::weak_ptr<Rendering> _rendering = {};
        std::weak_ptr<ScriptSystem> _script_system = {};

        uint64 _update_count = 0;
        double _time_running = 0;
        double _fixed_update_accumulator_seconds = 0.0;

        // Optional --live-together-die-together watchdog: monitors the launching process and
        // requests exit when it dies. Declared last so it is stopped/joined first on teardown,
        // before the members its callback touches. Default-constructed = no thread running.
        std::jthread _parent_watchdog = {};
    };

    using CreateAppFn = Application* (*)();
    using DestroyAppFn = void (*)(Application*);
}
