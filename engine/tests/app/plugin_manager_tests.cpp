#include "in_memory_file_ops.h"
#include "tbx/interfaces/input_manager.h"
#include "tbx/interfaces/physics_backend.h"
#include "tbx/systems/assets/manager.h"
#include "tbx/systems/async/job_system.h"
#include "tbx/systems/async/thread_manager.h"
#include "tbx/systems/ecs/registry.h"
#include "tbx/systems/ecs/world/manager.h"
#include "tbx/systems/messaging/message.h"
#include "tbx/systems/messaging/message_coordinator.h"
#include "tbx/systems/physics/physics.h"
#include "tbx/systems/plugin_api/plugin_manager.h"
#include "tbx/systems/scripting/service_ref.h"
#include "tbx/types/assets/builtin_assets.h"
#include <memory>
#include <type_traits>

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
        void on_attach() override
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
    };

    static void register_fake_physics_services(Plugin*, ServiceProvider* service_provider)
    {
        if (service_provider == nullptr)
            return;

        service_provider->register_service<IPhysicsBackend>(std::make_shared<FakePhysicsBackend>());
    }

    static void register_app_physics_service(ServiceProvider& service_provider)
    {
        service_provider.register_service<Physics>(std::make_shared<Physics>(
            service_provider.try_get_service<IPhysicsBackend>(),
            service_provider.try_get_service<AssetManager>(),
            service_provider.try_get_service<WorldManager>(),
            PhysicsSettings {}));
    }

    class PhysicsConsumerPlugin final : public Plugin
    {
      public:
        PhysicsConsumerPlugin(std::shared_ptr<TestPluginState> state)
            : _state(std::move(state))
        {
        }

      protected:
        void on_attach() override
        {
            ++_state->attach_count;
            _state->had_physics_on_attach = !physics.expired();
        }

        void on_detach() override
        {
            ++_state->detach_count;
        }

      public:
        std::weak_ptr<Physics> physics = {};

      private:
        std::shared_ptr<TestPluginState> _state = {};
    };

    static void bind_physics_consumer_runtime(Plugin* plugin, ServiceProvider* service_provider)
    {
        if (plugin == nullptr || service_provider == nullptr)
            return;

        auto* typed_plugin = dynamic_cast<PhysicsConsumerPlugin*>(plugin);
        if (typed_plugin == nullptr)
            return;

        bind_service_field(typed_plugin->physics, *service_provider);
    }

    static void populate_test_service_provider(
        ServiceProvider& service_provider,
        const std::filesystem::path& working_directory)
    {
        service_provider.register_service<IMessageCoordinator>(
            std::make_shared<MessageCoordinator>());
        service_provider.register_service<IFileOps>(
            std::make_shared<tbx::tests::InMemoryFileOps>(working_directory));
        service_provider.register_service<EntityRegistry>(std::make_shared<EntityRegistry>());
        service_provider.register_service<SerializationRegistry>(
            std::make_shared<SerializationRegistry>());
        auto message_coordinator = service_provider.get_service<IMessageCoordinator>().lock();
        auto serialization_registry = service_provider.get_service<SerializationRegistry>().lock();
        if (!message_coordinator || !serialization_registry)
            return;

        service_provider.register_service<AssetManager>(std::make_shared<AssetManager>(
            service_provider.get_service<IMessageCoordinator>(),
            service_provider.get_service<SerializationRegistry>(),
            working_directory,
            std::vector<std::filesystem::path> {}));
        service_provider.register_service<JobSystem>(std::make_shared<JobSystem>());
        service_provider.register_service<ThreadManager>(std::make_shared<ThreadManager>());
    }

    static LoadedPlugins make_loaded_plugin(
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
        auto plugins = LoadedPlugins {};
        plugins.emplace_back(meta, nullptr, std::move(instance));
        return plugins;
    }

    static LoadedPlugins make_fake_physics_backend_plugin()
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
        auto plugins = LoadedPlugins {};
        plugins.emplace_back(meta, nullptr, std::move(instance), register_fake_physics_services);
        return plugins;
    }

    static LoadedPlugins make_physics_consumer_plugin(std::shared_ptr<TestPluginState>& out_state)
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
        auto plugins = LoadedPlugins {};
        plugins.emplace_back(
            meta,
            nullptr,
            std::move(instance),
            nullptr,
            bind_physics_consumer_runtime);
        return plugins;
    }

    static_assert(!std::is_copy_constructible_v<LoadedPlugin>);
    static_assert(!std::is_copy_assignable_v<LoadedPlugin>);
    static_assert(!std::is_move_constructible_v<LoadedPlugin>);
    static_assert(!std::is_move_assignable_v<LoadedPlugin>);

    TEST(plugin_manager, routes_messages_during_plugin_attach)
    {
        // Arrange
        const std::filesystem::path working_directory = "/virtual/plugin_manager";
        auto service_provider = std::make_shared<ServiceProvider>();
        populate_test_service_provider(*service_provider, working_directory);
        auto file_ops = std::make_shared<tbx::tests::InMemoryFileOps>(working_directory);
        PluginManager manager = PluginManager(service_provider, file_ops);
        auto msg_coordinator = service_provider->get_service<IMessageCoordinator>().lock();
        ASSERT_NE(msg_coordinator, nullptr);
        msg_coordinator->register_handler(
            [&manager](Message& msg)
            {
                manager.receive_message(msg);
            });
        std::shared_ptr<TestPluginState> first = {};
        std::shared_ptr<TestPluginState> second = {};

        // Act
        manager.add(make_loaded_plugin("Alpha", first, true));
        manager.add(make_loaded_plugin("Beta", second, true));
        manager.attach_all();

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
        auto service_provider = std::make_shared<ServiceProvider>();
        populate_test_service_provider(*service_provider, working_directory);
        auto file_ops = std::make_shared<tbx::tests::InMemoryFileOps>(working_directory);
        PluginManager manager = PluginManager(service_provider, file_ops);
        std::shared_ptr<TestPluginState> consumer = {};

        // Act
        manager.add(make_fake_physics_backend_plugin());
        manager.add(make_physics_consumer_plugin(consumer));
        register_app_physics_service(*service_provider);
        manager.attach_all();

        // Assert
        ASSERT_NE(consumer, nullptr);
        EXPECT_TRUE(service_provider->has_service<Physics>());
        EXPECT_EQ(consumer->attach_count, 1);
        EXPECT_TRUE(consumer->had_physics_on_attach);
    }

    TEST(plugin_manager, unloads_specific_plugin_and_stops_routing_after_unload)
    {
        // Arrange
        const std::filesystem::path working_directory = "/virtual/plugin_manager";
        auto service_provider = std::make_shared<ServiceProvider>();
        populate_test_service_provider(*service_provider, working_directory);
        auto file_ops = std::make_shared<tbx::tests::InMemoryFileOps>(working_directory);
        PluginManager manager = PluginManager(service_provider, file_ops);
        auto msg_coordinator = service_provider->get_service<IMessageCoordinator>().lock();
        ASSERT_NE(msg_coordinator, nullptr);
        msg_coordinator->register_handler(
            [&manager](Message& msg)
            {
                manager.receive_message(msg);
            });
        std::shared_ptr<TestPluginState> plugin = {};
        manager.add(make_loaded_plugin("Solo", plugin, false, true));
        manager.attach_all();

        // Act
        manager.update(DeltaTime {.seconds = 0.016, .milliseconds = 16.0});
        manager.fixed_update(DeltaTime {.seconds = 0.008, .milliseconds = 8.0});
        msg_coordinator->send<PluginPingMessage>("before_shutdown");
        EXPECT_TRUE(manager.unload("Solo"));
        msg_coordinator->send<PluginPingMessage>("after_shutdown");

        // Assert
        ASSERT_NE(plugin, nullptr);
        EXPECT_EQ(plugin->update_count, 1);
        EXPECT_EQ(plugin->fixed_update_count, 1);
        ASSERT_EQ(plugin->received_sources.size(), 1U);
        EXPECT_EQ(plugin->received_sources[0], "before_shutdown");
        EXPECT_EQ(plugin->detach_count, 1);
    }

    TEST(plugin_manager, deregisters_plugin_owned_services_after_unload)
    {
        // Arrange
        const std::filesystem::path working_directory = "/virtual/plugin_manager";
        auto service_provider = std::make_shared<ServiceProvider>();
        populate_test_service_provider(*service_provider, working_directory);
        auto file_ops = std::make_shared<tbx::tests::InMemoryFileOps>(working_directory);
        PluginManager manager = PluginManager(service_provider, file_ops);

        // Act
        manager.add(make_fake_physics_backend_plugin());
        manager.attach_all();
        auto backend = service_provider->get_service<IPhysicsBackend>();
        EXPECT_TRUE(service_provider->has_service<IPhysicsBackend>());
        EXPECT_FALSE(backend.expired());
        EXPECT_TRUE(manager.unload("FakePhysicsBackend"));

        // Assert
        EXPECT_FALSE(service_provider->has_service<IPhysicsBackend>());
        EXPECT_TRUE(backend.expired());
    }

    TEST(plugin_manager, detached_plugins_do_not_update_fixed_update_or_receive_messages)
    {
        // Arrange
        const std::filesystem::path working_directory = "/virtual/plugin_manager";
        auto service_provider = std::make_shared<ServiceProvider>();
        populate_test_service_provider(*service_provider, working_directory);
        auto file_ops = std::make_shared<tbx::tests::InMemoryFileOps>(working_directory);
        PluginManager manager = PluginManager(service_provider, file_ops);
        auto msg_coordinator = service_provider->get_service<IMessageCoordinator>().lock();
        ASSERT_NE(msg_coordinator, nullptr);
        msg_coordinator->register_handler(
            [&manager](Message& msg)
            {
                manager.receive_message(msg);
            });
        std::shared_ptr<TestPluginState> plugin = {};
        manager.add(make_loaded_plugin("Detached", plugin));
        manager.attach_all();
        manager.detach_all();

        // Act
        manager.update(DeltaTime {.seconds = 0.016, .milliseconds = 16.0});
        manager.fixed_update(DeltaTime {.seconds = 0.008, .milliseconds = 8.0});
        msg_coordinator->send<PluginPingMessage>("after_detach");

        // Assert
        ASSERT_NE(plugin, nullptr);
        EXPECT_EQ(plugin->detach_count, 1);
        EXPECT_EQ(plugin->update_count, 0);
        EXPECT_EQ(plugin->fixed_update_count, 0);
        EXPECT_TRUE(plugin->received_sources.empty());
    }

#if defined(TBX_ASSERTS_ENABLED)
    TEST(plugin_manager, unload_asserts_when_plugin_owned_service_has_external_strong_reference)
    {
        // Arrange
        const std::filesystem::path working_directory = "/virtual/plugin_manager";
        auto service_provider = std::make_shared<ServiceProvider>();
        populate_test_service_provider(*service_provider, working_directory);
        auto file_ops = std::make_shared<tbx::tests::InMemoryFileOps>(working_directory);
        PluginManager manager = PluginManager(service_provider, file_ops);
        manager.add(make_fake_physics_backend_plugin());
        manager.attach_all();
        auto retained_backend = service_provider->get_service<IPhysicsBackend>().lock();
        ASSERT_NE(retained_backend, nullptr);

        // Act / Assert
        EXPECT_DEATH_IF_SUPPORTED({ static_cast<void>(manager.unload("FakePhysicsBackend")); }, "");

        retained_backend.reset();
        EXPECT_TRUE(manager.unload("FakePhysicsBackend"));
    }
#endif
}
