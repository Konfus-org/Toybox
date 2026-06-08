#define private public
#include "tbx/systems/app/application.h"
#undef private

#include "in_memory_file_ops.h"
#include "tbx/interfaces/window_backend.h"
#include "tbx/systems/app/messages.h"
#include "tbx/systems/messaging/message_coordinator.h"
#include "tbx/systems/plugin_api/plugin_manager.h"
#include "tbx/systems/plugin_api/service_provider.h"
#include <cstdio>

namespace tbx::tests::app
{
    class RecordingWindowBackend final : public IWindowBackend
    {
      public:
        void initialize() override
        {
            initialized = true;
        }

        bool create_window(
            const Window& window,
            const WindowCreateInfo& create_info,
            NativeWindowHandle& out_native_handle) override
        {
            created_windows.push_back(window);
            created_titles.push_back(create_info.title);
            out_native_handle = reinterpret_cast<NativeWindowHandle>(1);
            return true;
        }

        bool destroy_window(const Window&) override
        {
            return true;
        }

        bool set_window_mode(const Window&, WindowMode) override
        {
            return true;
        }

        bool set_window_title(const Window&, const std::string&) override
        {
            return true;
        }

        bool set_window_size(const Window&, const Size&) override
        {
            return true;
        }

        void pump_events(std::vector<WindowBackendEvent>&) override {}

        void shutdown() override
        {
            initialized = false;
            created_windows.clear();
            created_titles.clear();
        }

      public:
        std::vector<Window> created_windows = {};
        std::vector<std::string> created_titles = {};
        bool initialized = false;
    };

    class TestApplication final : public Application
    {
      public:
        using Application::initialize;
        using Application::shutdown;
    };

    static CommandList make_command_list(const char* settings_path)
    {
        char program[] = "ToyboxLauncher.exe";
        char settings_arg[64] = {};
        std::snprintf(settings_arg, sizeof(settings_arg), "--settings=%s", settings_path);
        char* argv[] = {program, settings_arg};
        return CommandList(2, argv);
    }

    TEST(application, initialize_loads_settings_and_opens_main_window_when_runtime_services_exist)
    {
        // Arrange
        auto file_ops = std::make_shared<InMemoryFileOps>(std::filesystem::path());
        file_ops->set_text(
            "Settings.json",
            R"({
                "name": "Init Success App",
                "plugins": []
            })");
        auto service_provider = std::make_shared<ServiceProvider>();
        service_provider->register_service<IFileOps>(file_ops);
        register_default_services(*service_provider);
        auto backend = std::make_shared<RecordingWindowBackend>();
        service_provider->register_service<IWindowBackend>(backend);

        auto initialized = false;
        auto coordinator = service_provider->get_service<IMessageCoordinator>().lock();
        ASSERT_NE(coordinator, nullptr);
        coordinator->register_handler(
            [&](Message& msg)
            {
                if (handle_message<ApplicationInitializedEvent>(msg).has_value())
                    initialized = true;
            });

        auto application = TestApplication();
        application._service_provider = service_provider;
        application._plugin_manager = std::make_unique<PluginManager>(service_provider, file_ops);
        const auto command_list = make_command_list("Settings.json");

        // Act
        const int result = application.initialize(command_list, std::filesystem::path());

        // Assert
        EXPECT_EQ(result, 0);
        EXPECT_TRUE(initialized);
        ASSERT_EQ(backend->created_windows.size(), 1U);
        EXPECT_EQ(backend->created_titles.front(), "Init Success App");
        EXPECT_FALSE(application._window_manager.expired());
        EXPECT_TRUE(application.get_main_window().id.is_valid());

        application.shutdown();
    }

    TEST(application, initialize_fails_when_window_backend_is_missing)
    {
        // Arrange
        auto file_ops = std::make_shared<InMemoryFileOps>(std::filesystem::path());
        file_ops->set_text(
            "Settings.json",
            R"({
                "name": "Init Failure App",
                "plugins": []
            })");
        auto service_provider = std::make_shared<ServiceProvider>();
        service_provider->register_service<IFileOps>(file_ops);
        register_default_services(*service_provider);

        auto initialized = false;
        auto coordinator = service_provider->get_service<IMessageCoordinator>().lock();
        ASSERT_NE(coordinator, nullptr);
        coordinator->register_handler(
            [&](Message& msg)
            {
                if (handle_message<ApplicationInitializedEvent>(msg).has_value())
                    initialized = true;
            });

        auto application = TestApplication();
        application._service_provider = service_provider;
        application._plugin_manager = std::make_unique<PluginManager>(service_provider, file_ops);
        const auto command_list = make_command_list("Settings.json");

        // Act
        const int result = application.initialize(command_list, std::filesystem::path());

        // Assert
        EXPECT_EQ(result, -1);
        EXPECT_FALSE(initialized);
        EXPECT_TRUE(application._window_manager.expired());
        EXPECT_FALSE(application.get_main_window().id.is_valid());

        application.shutdown();
    }
}
