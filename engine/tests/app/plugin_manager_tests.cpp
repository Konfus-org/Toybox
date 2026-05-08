#include "pch.h"
#include "tbx/core/systems/files/tests/in_memory_file_ops.h"
#include "tbx/interfaces/input_manager.h"
#include "tbx/interfaces/physics_backend.h"
#include "tbx/systems/app/settings.h"
#include "tbx/systems/assets/builtin_assets.h"
#include "tbx/systems/assets/manager.h"
#include "tbx/systems/async/job_system.h"
#include "tbx/systems/async/thread_manager.h"
#include "tbx/systems/ecs/entity_registry.h"
#include "tbx/systems/messages/message_coordinator.h"
#include "tbx/systems/messaging/message.h"
#include "tbx/systems/physics/physics.h"
#include "tbx/systems/plugin_api/plugin_manager.h"
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
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
        bool had_physics_on_attach = false;
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
        void on_attach(ServiceProvider&) override
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
            auto ping = handle_message<PluginPingMessage>(msg);
            if (!ping.has_value())
                return;

            ++_state->receive_count;
            _state->received_sources.push_back(ping->get().source);
        }

        void on_update(const DeltaTime&) override
        {
            ++_state->update_count;
        }

      private:
        std::shared_ptr<TestPluginState> _state = {};
    };

    class FakePhysicsBackend final : public IPhysicsBackend
    {
      public:
        void initialize(const PhysicsBackendSettings&) override {}

        void shutdown() override {}

        void update(const PhysicsBackendSettings&, const DeltaTime&) override {}

        bool raycast(const RaycastQuery&, PhysicsRigidbodyHandle, PhysicsRaycastHit&) const override
        {
            return false;
        }

        PhysicsColliderHandle create_collider(const PhysicsColliderCreateInfo&) override
        {
            return {};
        }

        void destroy_collider(PhysicsColliderHandle) override {}

        void update_collider(PhysicsColliderHandle, const PhysicsColliderCreateInfo&) override {}

        PhysicsRigidbodyHandle create_rigidbody(const PhysicsRigidbodyCreateInfo&) override
        {
            return {};
        }

        void destroy_rigidbody(PhysicsRigidbodyHandle) override {}

        PhysicsRigidbodyState get_rigidbody_state(PhysicsRigidbodyHandle) const override
        {
            return {};
        }

        void get_rigidbody_overlaps(PhysicsRigidbodyHandle, std::vector<PhysicsRigidbodyHandle>&)
            const override
        {
        }

        void update_rigidbody(PhysicsRigidbodyHandle, const PhysicsRigidbodyUpdateInfo&) override {}
    };

    class FakePhysicsBackendPlugin final : public Plugin
    {
      protected:
        void on_attach(ServiceProvider& service_provider) override
        {
            _service_provider = std::ref(service_provider);
            service_provider.register_service<IPhysicsBackend>(
                std::make_unique<FakePhysicsBackend>());
        }

        void on_detach() override
        {
            if (_service_provider.has_value() && _service_provider->get().has_service<Physics>())
                _service_provider->get().deregister_service<Physics>();

            if (_service_provider.has_value()
                && _service_provider->get().has_service<IPhysicsBackend>())
                _service_provider->get().deregister_service<IPhysicsBackend>();

            _service_provider = std::nullopt;
        }

      private:
        std::optional<std::reference_wrapper<ServiceProvider>> _service_provider = std::nullopt;
    };

    class PhysicsConsumerPlugin final : public Plugin
    {
      public:
        PhysicsConsumerPlugin(std::shared_ptr<TestPluginState> state)
            : _state(std::move(state))
        {
        }

      protected:
        void on_attach(ServiceProvider& service_provider) override
        {
            ++_state->attach_count;
            _state->had_physics_on_attach = service_provider.try_get_service<Physics>().has_value();
        }

        void on_detach() override
        {
            ++_state->detach_count;
        }

      private:
        std::shared_ptr<TestPluginState> _state = {};
    };

    static ServiceProvider make_test_service_provider(
        const std::filesystem::path& working_directory)
    {
        auto service_provider = ServiceProvider {};

        service_provider.register_service<IMessageCoordinator>(
            std::make_unique<MessageCoordinator>());
        service_provider.register_service<EntityRegistry>(std::make_unique<EntityRegistry>());
        service_provider.register_service<SerializationRegistry>(
            std::make_unique<SerializationRegistry>());
        service_provider.register_service<AssetManager>(std::make_unique<AssetManager>(
            service_provider.get_service<IMessageCoordinator>(),
            service_provider.get_service<SerializationRegistry>(),
            working_directory,
            std::vector<std::filesystem::path> {}));
        service_provider.register_service<AppSettings>(std::make_unique<AppSettings>(
            service_provider.get_service<IMessageCoordinator>(),
            true,
            GraphicsApi::OPEN_GL,
            Size {1280, 720}));
        auto& settings = service_provider.get_service<AppSettings>();
        settings.paths.working_directory = working_directory;
        settings.paths.logs_directory = working_directory / "logs";
        settings.icon = ToyboxIcon::HANDLE;
        service_provider.register_service<JobSystem>(std::make_unique<JobSystem>());
        service_provider.register_service<ThreadManager>(std::make_unique<ThreadManager>());

        return service_provider;
    }

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

    static LoadedPlugin make_fake_physics_backend_plugin()
    {
        PluginMeta meta = {};
        meta.name = "FakePhysicsBackend";
        meta.version = "1.0.0";
        meta.abi_version = PluginAbiVersion;
        meta.category = PluginCategory::PHYSICS;

        auto instance = std::unique_ptr<Plugin, PluginDeleter>(
            new FakePhysicsBackendPlugin(),
            [](Plugin* plugin)
            {
                delete plugin;
            });
        return LoadedPlugin(meta, {}, std::move(instance));
    }

    static LoadedPlugin make_physics_consumer_plugin(std::shared_ptr<TestPluginState>& out_state)
    {
        PluginMeta meta = {};
        meta.name = "PhysicsConsumer";
        meta.version = "1.0.0";
        meta.abi_version = PluginAbiVersion;
        meta.category = PluginCategory::GAMEPLAY;
        meta.dependencies.push_back("FakePhysicsBackend");

        out_state = std::make_shared<TestPluginState>();
        out_state->name = meta.name;

        auto instance = std::unique_ptr<Plugin, PluginDeleter>(
            new PhysicsConsumerPlugin(out_state),
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
        auto service_provider = make_test_service_provider(working_directory);
        auto file_ops =
            std::make_shared<tbx::tests::file_system::InMemoryFileOps>(working_directory);
        PluginManager manager = PluginManager(service_provider, file_ops);
        service_provider.get_service<IMessageCoordinator>().register_handler(
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

    TEST(plugin_manager, registers_physics_service_after_backend_plugin_attach)
    {
        // Arrange
        const std::filesystem::path working_directory = "/virtual/plugin_manager";
        auto service_provider = make_test_service_provider(working_directory);
        auto file_ops =
            std::make_shared<tbx::tests::file_system::InMemoryFileOps>(working_directory);
        PluginManager manager = PluginManager(service_provider, file_ops);
        std::shared_ptr<TestPluginState> consumer = {};

        // Act
        manager.add(make_fake_physics_backend_plugin());
        manager.add(make_physics_consumer_plugin(consumer));

        // Assert
        ASSERT_NE(consumer, nullptr);
        EXPECT_TRUE(service_provider.has_service<Physics>());
        EXPECT_EQ(consumer->attach_count, 1);
        EXPECT_TRUE(consumer->had_physics_on_attach);
    }

    TEST(plugin_manager, unloads_specific_plugin_and_stops_routing_after_unload)
    {
        // Arrange
        const std::filesystem::path working_directory = "/virtual/plugin_manager";
        auto service_provider = make_test_service_provider(working_directory);
        auto file_ops =
            std::make_shared<tbx::tests::file_system::InMemoryFileOps>(working_directory);
        PluginManager manager = PluginManager(service_provider, file_ops);
        service_provider.get_service<IMessageCoordinator>().register_handler(
            [&manager](Message& msg)
            {
                manager.receive_message(msg);
            });
        std::shared_ptr<TestPluginState> plugin = {};
        manager.add(make_loaded_plugin("Solo", plugin, false, true));

        // Act
        manager.update(DeltaTime {.seconds = 0.016, .milliseconds = 16.0});
        manager.fixed_update(DeltaTime {.seconds = 0.008, .milliseconds = 8.0});
        service_provider.get_service<IMessageCoordinator>().send<PluginPingMessage>(
            "before_shutdown");
        EXPECT_TRUE(manager.unload("Solo"));
        service_provider.get_service<IMessageCoordinator>().send<PluginPingMessage>(
            "after_shutdown");

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
