#include "pch.h"
#include "tbx/app/app_message_coordinator.h"
#include "tbx/app/app_settings.h"
#include "tbx/app/plugin_manager.h"
#include "tbx/assets/asset_manager.h"
#include "tbx/assets/builtin_assets.h"
#include "tbx/files/tests/in_memory_file_ops.h"
#include "tbx/messages/message.h"
#include <filesystem>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace tbx::tests::app
{
    struct TestPluginState
    {
        std::string name = {};
        bool emit_attach_message = false;
        bool emit_detach_message = false;
        int attach_count = 0;
        int detach_count = 0;
        int fixed_update_count = 0;
        int receive_count = 0;
        int update_count = 0;
        std::vector<std::string> received_sources = {};
    };

    struct PluginPingMessage : public Message
    {
        PluginPingMessage() = default;

        PluginPingMessage(std::string source_name)
            : source(std::move(source_name))
        {
        }

        std::string source = {};
    };

    class TestPlugin final : public Plugin
    {
      public:
        TestPlugin(std::shared_ptr<TestPluginState> state)
            : _state(std::move(state))
        {
        }

      protected:
        void on_attach(IPluginHost&) override
        {
            ++_state->attach_count;
            if (_state->emit_attach_message)
                send_message<PluginPingMessage>(_state->name);
        }

        void on_detach() override
        {
            ++_state->detach_count;
            if (_state->emit_detach_message)
                send_message<PluginPingMessage>(_state->name + "_detach");
        }

        void on_fixed_update(const DeltaTime&) override
        {
            ++_state->fixed_update_count;
        }

        void on_recieve_message(Message& msg) override
        {
            auto* ping = handle_message<PluginPingMessage>(msg);
            if (!ping)
                return;

            ++_state->receive_count;
            _state->received_sources.push_back(ping->source);
        }

        void on_update(const DeltaTime&) override
        {
            ++_state->update_count;
        }

      private:
        std::shared_ptr<TestPluginState> _state = {};
    };

    class TestPluginHost final : public IPluginHost
    {
      public:
        TestPluginHost(const std::filesystem::path& working_directory)
            : _asset_manager(&_coordinator, working_directory)
            , _settings(_coordinator, true, GraphicsApi::OPEN_GL, {1280, 720})
        {
            _settings.paths.working_directory = working_directory;
            _settings.paths.logs_directory = working_directory / "logs";
        }

      public:
        const std::string& get_name() const override
        {
            return _name;
        }

        const Handle& get_icon_handle() const override
        {
            return _icon_handle;
        }

        AppSettings& get_settings() override
        {
            return _settings;
        }

        IMessageCoordinator& get_message_coordinator() override
        {
            return _coordinator;
        }

        InputManager& get_input_manager() override
        {
            return _input_manager;
        }

        EntityRegistry& get_entity_registry() override
        {
            return _entity_registry;
        }

        AssetManager& get_asset_manager() override
        {
            return _asset_manager;
        }

        JobSystem& get_job_system() override
        {
            return _job_manager;
        }

        ThreadManager& get_thread_manager() override
        {
            return _thread_manager;
        }

      private:
        std::string _name = "PluginManagerTests";
        Handle _icon_handle = ToyboxIcon::HANDLE;
        AppMessageCoordinator _coordinator = {};
        InputManager _input_manager = _coordinator;
        EntityRegistry _entity_registry = {};
        AssetManager _asset_manager;
        AppSettings _settings;
        JobSystem _job_manager;
        ThreadManager _thread_manager;
    };

    static LoadedPlugin make_loaded_plugin(
        const std::string& name,
        std::shared_ptr<TestPluginState>& out_state,
        bool emit_attach_message = false,
        bool emit_detach_message = false)
    {
        PluginMeta meta = {};
        meta.name = name;
        meta.version = "1.0.0";
        meta.abi_version = PluginAbiVersion;
        out_state = std::make_shared<TestPluginState>();
        out_state->name = name;
        out_state->emit_attach_message = emit_attach_message;
        out_state->emit_detach_message = emit_detach_message;

        auto instance = std::unique_ptr<Plugin, PluginDeleter>(
            new TestPlugin(out_state),
            [](Plugin* plugin)
            {
                delete plugin;
            });
        return LoadedPlugin(meta, {}, std::move(instance));
    }

    TEST(plugin_manager, routes_messages_during_plugin_attach)
    {
        // Arrange
        const std::filesystem::path working_directory = "/virtual/plugin_manager";
        TestPluginHost host = TestPluginHost(working_directory);
        auto file_ops = std::make_shared<tbx::tests::file_system::InMemoryFileOps>(working_directory);
        PluginManager manager = PluginManager(host, file_ops);
        host.get_message_coordinator().register_handler(
            [&manager](Message& msg)
            {
                manager.receive_message(msg);
            });
        std::shared_ptr<TestPluginState> first = {};
        std::shared_ptr<TestPluginState> second = {};

        // Act
        manager.add(make_loaded_plugin("Alpha", first, true));
        manager.add(make_loaded_plugin("Beta", second, true));

        // Assert
        ASSERT_NE(first, nullptr);
        ASSERT_NE(second, nullptr);
        EXPECT_EQ(first->attach_count, 1);
        EXPECT_EQ(second->attach_count, 1);
        ASSERT_EQ(first->received_sources.size(), 2U);
        EXPECT_EQ(first->received_sources[0], "Alpha");
        EXPECT_EQ(first->received_sources[1], "Beta");
        ASSERT_EQ(second->received_sources.size(), 1U);
        EXPECT_EQ(second->received_sources[0], "Beta");
    }

    TEST(plugin_manager, unloads_specific_plugin_and_stops_routing_after_unload)
    {
        // Arrange
        const std::filesystem::path working_directory = "/virtual/plugin_manager";
        TestPluginHost host = TestPluginHost(working_directory);
        auto file_ops = std::make_shared<tbx::tests::file_system::InMemoryFileOps>(working_directory);
        PluginManager manager = PluginManager(host, file_ops);
        host.get_message_coordinator().register_handler(
            [&manager](Message& msg)
            {
                manager.receive_message(msg);
            });
        std::shared_ptr<TestPluginState> plugin = {};
        manager.add(make_loaded_plugin("Solo", plugin, false, true));

        // Act
        manager.update(DeltaTime {.seconds = 0.016, .milliseconds = 16.0});
        manager.fixed_update(DeltaTime {.seconds = 0.008, .milliseconds = 8.0});
        host.get_message_coordinator().send<PluginPingMessage>("before_shutdown");
        EXPECT_TRUE(manager.unload("Solo"));
        host.get_message_coordinator().send<PluginPingMessage>("after_shutdown");

        // Assert
        ASSERT_NE(plugin, nullptr);
        EXPECT_EQ(plugin->update_count, 1);
        EXPECT_EQ(plugin->fixed_update_count, 1);
        ASSERT_EQ(plugin->received_sources.size(), 2U);
        EXPECT_EQ(plugin->received_sources[0], "before_shutdown");
        EXPECT_EQ(plugin->received_sources[1], "Solo_detach");
        EXPECT_EQ(plugin->detach_count, 1);
    }
}
