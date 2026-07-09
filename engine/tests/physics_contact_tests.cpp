#include "tbx/interfaces/file_ops.h"
#include "tbx/interfaces/physics_backend.h"
#include "tbx/systems/assets/manager.h"
#include "tbx/systems/messaging/message_coordinator.h"
#include "tbx/systems/physics/physics.h"
#include "tbx/types/assets/world.h"
#include "tbx/types/components/collider.h"
#include "tbx/types/components/transform.h"
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace
{
    // In-memory stand-in so the asset manager never touches the real filesystem.
    class FakeFileOps final : public tbx::IFileOps
    {
      public:
        std::filesystem::path get_working_directory() const override
        {
            return {};
        }

        std::filesystem::path resolve(const std::filesystem::path& path) const override
        {
            return path;
        }

        bool exists(const std::filesystem::path&) const override
        {
            return false;
        }

        tbx::FileType get_type(const std::filesystem::path&) const override
        {
            return tbx::FileType::NONE;
        }

        std::filesystem::file_time_type get_last_write_time(
            const std::filesystem::path&) const override
        {
            return {};
        }

        std::vector<std::filesystem::path> read_directory(
            const std::filesystem::path&) const override
        {
            return {};
        }

        bool read_file(const std::filesystem::path&, tbx::FileDataFormat, std::string&)
            const override
        {
            return false;
        }

        bool write_file(const std::filesystem::path&, tbx::FileDataFormat, const std::string&)
            override
        {
            return false;
        }
    };

    // Scripted physics backend: hands out sequential handles, remembers which rigidbody was
    // created at which x position, and drains whatever contact events the test queued.
    class FakePhysicsBackend final : public tbx::IPhysicsBackend
    {
      public:
        void initialize(const tbx::PhysicsBackendSettings&) override
        {
        }

        void shutdown() override
        {
        }

        void update(const tbx::PhysicsBackendSettings&, const tbx::DeltaTime&) override
        {
        }

        void drain_contact_events(std::vector<tbx::PhysicsContactEvent>& out_events) override
        {
            out_events.insert(
                out_events.end(),
                pending_contact_events.begin(),
                pending_contact_events.end());
            pending_contact_events.clear();
        }

        bool raycast(
            const tbx::RaycastQuery&,
            tbx::PhysicsRigidbodyHandle,
            tbx::PhysicsRaycastHit&) const override
        {
            return false;
        }

        tbx::PhysicsColliderHandle create_collider(const tbx::PhysicsColliderCreateInfo&) override
        {
            return tbx::PhysicsColliderHandle {.value = next_handle_value++};
        }

        void destroy_collider(tbx::PhysicsColliderHandle) override
        {
        }

        void update_collider(tbx::PhysicsColliderHandle, const tbx::PhysicsColliderCreateInfo&)
            override
        {
        }

        tbx::PhysicsRigidbodyHandle create_rigidbody(
            const tbx::PhysicsRigidbodyCreateInfo& create_info) override
        {
            const auto handle = tbx::PhysicsRigidbodyHandle {.value = next_handle_value++};
            created_rigidbodies.push_back(handle);
            rigidbody_by_position_x[create_info.transform.position.x] = handle;
            return handle;
        }

        void destroy_rigidbody(tbx::PhysicsRigidbodyHandle) override
        {
        }

        tbx::PhysicsRigidbodyState get_rigidbody_state(tbx::PhysicsRigidbodyHandle) const override
        {
            return {};
        }

        void get_rigidbody_overlaps(
            tbx::PhysicsRigidbodyHandle rigidbody,
            std::vector<tbx::PhysicsRigidbodyHandle>& out_overlaps) const override
        {
            out_overlaps.clear();
            for (const tbx::PhysicsRigidbodyHandle& other : created_rigidbodies)
            {
                if (!(other == rigidbody))
                    out_overlaps.push_back(other);
            }
        }

        void update_rigidbody(
            tbx::PhysicsRigidbodyHandle,
            const tbx::PhysicsRigidbodyUpdateInfo&) override
        {
        }

      public:
        std::vector<tbx::PhysicsContactEvent> pending_contact_events = {};
        std::vector<tbx::PhysicsRigidbodyHandle> created_rigidbodies = {};
        std::unordered_map<float, tbx::PhysicsRigidbodyHandle> rigidbody_by_position_x = {};
        uint64 next_handle_value = 1U;
    };

    // Owns the service graph a Physics instance needs; every dependency is in-memory.
    struct PhysicsTestRig
    {
        PhysicsTestRig()
            : file_ops(std::make_shared<FakeFileOps>())
            , backend(std::make_shared<FakePhysicsBackend>())
            , coordinator(std::make_shared<tbx::MessageCoordinator>())
            , asset_manager(std::make_shared<tbx::AssetManager>(
                  std::weak_ptr<tbx::IMessageDispatcher>(),
                  std::weak_ptr<tbx::SerializationRegistry>(),
                  std::filesystem::path(),
                  std::vector<std::filesystem::path>(),
                  tbx::HandleSource(),
                  file_ops))
        {
            world = asset_manager->get_or_register<tbx::World>(
                tbx::Handle(tbx::Uuid::generate()),
                []() { return std::make_shared<tbx::World>(); });
        }

        tbx::Entity create_collider_entity(const std::string& name, float position_x)
        {
            auto entity = world->create_entity(name);
            auto transform = tbx::Transform {};
            transform.position = tbx::Vec3(position_x, 0.0F, 0.0F);
            entity.add_component<tbx::Transform>(transform);
            entity.add_component<tbx::BoxCollider>();
            return entity;
        }

        std::shared_ptr<FakeFileOps> file_ops;
        std::shared_ptr<FakePhysicsBackend> backend;
        std::shared_ptr<tbx::MessageCoordinator> coordinator;
        std::shared_ptr<tbx::AssetManager> asset_manager;
        std::shared_ptr<tbx::World> world;
    };

    constexpr tbx::DeltaTime STEP_DELTA = tbx::DeltaTime {.seconds = 0.016, .milliseconds = 16.0};

    TEST(PhysicsContactTests, BeginContactInvokesCallbacksOnBothEntities)
    {
        auto rig = PhysicsTestRig();
        auto entity_a = rig.create_collider_entity("a", 1.0F);
        auto entity_b = rig.create_collider_entity("b", 2.0F);

        auto physics = tbx::Physics(
            rig.backend,
            rig.asset_manager,
            std::weak_ptr<tbx::WorldManager>(),
            std::weak_ptr<tbx::ThreadManager>(),
            std::weak_ptr<tbx::JobSystem>(),
            rig.coordinator,
            tbx::PhysicsSettings {});

        auto events_on_a = std::vector<tbx::ColliderContactEvent>();
        auto events_on_b = std::vector<tbx::ColliderContactEvent>();
        entity_a.get_component<tbx::BoxCollider>().contact_begin_callbacks.push_back(
            [&events_on_a](const tbx::ColliderContactEvent& event)
            { events_on_a.push_back(event); });
        entity_b.get_component<tbx::BoxCollider>().contact_begin_callbacks.push_back(
            [&events_on_b](const tbx::ColliderContactEvent& event)
            { events_on_b.push_back(event); });

        // First update registers the bodies with the backend and marks results pending.
        physics.update(STEP_DELTA, tbx::PhysicsSettings {});
        rig.backend->pending_contact_events.push_back(
            tbx::PhysicsContactEvent {
                .rigidbody_a = rig.backend->rigidbody_by_position_x.at(1.0F),
                .rigidbody_b = rig.backend->rigidbody_by_position_x.at(2.0F),
                .position = tbx::Vec3(0.5F, 1.0F, 2.0F),
                .normal = tbx::Vec3(0.0F, 1.0F, 0.0F),
                .phase = tbx::PhysicsContactPhase::BEGIN,
            });

        // Second update commits the step results and drains the buffered contact.
        physics.update(STEP_DELTA, tbx::PhysicsSettings {});

        ASSERT_EQ(events_on_a.size(), 1U);
        EXPECT_EQ(events_on_a[0].entity_id, entity_a.get_id());
        EXPECT_EQ(events_on_a[0].other_entity_id, entity_b.get_id());
        EXPECT_EQ(events_on_a[0].position, tbx::Vec3(0.5F, 1.0F, 2.0F));
        EXPECT_EQ(events_on_a[0].normal, tbx::Vec3(0.0F, 1.0F, 0.0F));

        ASSERT_EQ(events_on_b.size(), 1U);
        EXPECT_EQ(events_on_b[0].entity_id, entity_b.get_id());
        EXPECT_EQ(events_on_b[0].other_entity_id, entity_a.get_id());
    }

    TEST(PhysicsContactTests, ContactsWithUnknownRigidbodiesAreDroppedWithoutCallbacks)
    {
        auto rig = PhysicsTestRig();
        auto entity_a = rig.create_collider_entity("a", 1.0F);

        auto physics = tbx::Physics(
            rig.backend,
            rig.asset_manager,
            std::weak_ptr<tbx::WorldManager>(),
            std::weak_ptr<tbx::ThreadManager>(),
            std::weak_ptr<tbx::JobSystem>(),
            rig.coordinator,
            tbx::PhysicsSettings {});

        auto callback_count = 0;
        entity_a.get_component<tbx::BoxCollider>().contact_begin_callbacks.push_back(
            [&callback_count](const tbx::ColliderContactEvent&) { ++callback_count; });

        physics.update(STEP_DELTA, tbx::PhysicsSettings {});
        // The second body was never registered with Physics (e.g. destroyed between the step and
        // the drain), so the whole event must be dropped gracefully.
        rig.backend->pending_contact_events.push_back(
            tbx::PhysicsContactEvent {
                .rigidbody_a = rig.backend->rigidbody_by_position_x.at(1.0F),
                .rigidbody_b = tbx::PhysicsRigidbodyHandle {.value = 999U},
                .phase = tbx::PhysicsContactPhase::BEGIN,
            });

        physics.update(STEP_DELTA, tbx::PhysicsSettings {});

        EXPECT_EQ(callback_count, 0);
    }

    TEST(PhysicsContactTests, EndContactInvokesEndCallbacks)
    {
        auto rig = PhysicsTestRig();
        auto entity_a = rig.create_collider_entity("a", 1.0F);
        auto entity_b = rig.create_collider_entity("b", 2.0F);

        auto physics = tbx::Physics(
            rig.backend,
            rig.asset_manager,
            std::weak_ptr<tbx::WorldManager>(),
            std::weak_ptr<tbx::ThreadManager>(),
            std::weak_ptr<tbx::JobSystem>(),
            rig.coordinator,
            tbx::PhysicsSettings {});

        auto begin_count = 0;
        auto end_events = std::vector<tbx::ColliderContactEvent>();
        auto& collider = entity_a.get_component<tbx::BoxCollider>();
        collider.contact_begin_callbacks.push_back(
            [&begin_count](const tbx::ColliderContactEvent&) { ++begin_count; });
        collider.contact_end_callbacks.push_back(
            [&end_events](const tbx::ColliderContactEvent& event) { end_events.push_back(event); });

        physics.update(STEP_DELTA, tbx::PhysicsSettings {});
        rig.backend->pending_contact_events.push_back(
            tbx::PhysicsContactEvent {
                .rigidbody_a = rig.backend->rigidbody_by_position_x.at(1.0F),
                .rigidbody_b = rig.backend->rigidbody_by_position_x.at(2.0F),
                .phase = tbx::PhysicsContactPhase::END,
            });

        physics.update(STEP_DELTA, tbx::PhysicsSettings {});

        EXPECT_EQ(begin_count, 0);
        ASSERT_EQ(end_events.size(), 1U);
        EXPECT_EQ(end_events[0].entity_id, entity_a.get_id());
        EXPECT_EQ(end_events[0].other_entity_id, entity_b.get_id());
    }

    TEST(PhysicsContactTests, ContactsDispatchSafelyWithoutMessageCoordinator)
    {
        auto rig = PhysicsTestRig();
        auto entity_a = rig.create_collider_entity("a", 1.0F);
        rig.create_collider_entity("b", 2.0F);

        // The coordinator-less constructor: callbacks still fire, message dispatch is skipped.
        auto physics = tbx::Physics(
            rig.backend,
            rig.asset_manager,
            std::weak_ptr<tbx::WorldManager>(),
            std::weak_ptr<tbx::ThreadManager>(),
            std::weak_ptr<tbx::JobSystem>(),
            tbx::PhysicsSettings {});

        auto callback_count = 0;
        entity_a.get_component<tbx::BoxCollider>().contact_begin_callbacks.push_back(
            [&callback_count](const tbx::ColliderContactEvent&) { ++callback_count; });

        physics.update(STEP_DELTA, tbx::PhysicsSettings {});
        rig.backend->pending_contact_events.push_back(
            tbx::PhysicsContactEvent {
                .rigidbody_a = rig.backend->rigidbody_by_position_x.at(1.0F),
                .rigidbody_b = rig.backend->rigidbody_by_position_x.at(2.0F),
                .phase = tbx::PhysicsContactPhase::BEGIN,
            });

        physics.update(STEP_DELTA, tbx::PhysicsSettings {});

        EXPECT_EQ(callback_count, 1);
    }

    TEST(PhysicsOverlapTests, TriggerOverlapInvokesBeginThenStayCallbacks)
    {
        auto rig = PhysicsTestRig();
        auto trigger_entity = rig.world->create_entity("trigger");
        trigger_entity.add_component<tbx::Transform>();
        trigger_entity.add_component<tbx::BoxTrigger>();
        auto other_entity = rig.create_collider_entity("other", 2.0F);

        auto physics = tbx::Physics(
            rig.backend,
            rig.asset_manager,
            std::weak_ptr<tbx::WorldManager>(),
            std::weak_ptr<tbx::ThreadManager>(),
            std::weak_ptr<tbx::JobSystem>(),
            rig.coordinator,
            tbx::PhysicsSettings {});

        auto begin_events = std::vector<tbx::ColliderOverlapEvent>();
        auto stay_events = std::vector<tbx::ColliderOverlapEvent>();
        auto& trigger = trigger_entity.get_component<tbx::BoxTrigger>();
        trigger.overlap_begin_callbacks.push_back(
            [&begin_events](const tbx::ColliderOverlapEvent& event)
            { begin_events.push_back(event); });
        trigger.overlap_stay_callbacks.push_back(
            [&stay_events](const tbx::ColliderOverlapEvent& event)
            { stay_events.push_back(event); });

        // The fake backend reports every other body as overlapping, so the second update (the one
        // that commits results) sees a fresh overlap and the third sees it persist.
        physics.update(STEP_DELTA, tbx::PhysicsSettings {});
        physics.update(STEP_DELTA, tbx::PhysicsSettings {});
        physics.update(STEP_DELTA, tbx::PhysicsSettings {});

        ASSERT_EQ(begin_events.size(), 1U);
        EXPECT_EQ(begin_events[0].trigger_entity_id, trigger_entity.get_id());
        EXPECT_EQ(begin_events[0].overlapped_entity_id, other_entity.get_id());
        ASSERT_EQ(stay_events.size(), 1U);
        EXPECT_EQ(stay_events[0].trigger_entity_id, trigger_entity.get_id());
        EXPECT_EQ(stay_events[0].overlapped_entity_id, other_entity.get_id());
    }

    TEST(PhysicsOverlapTests, DisabledTriggerInvokesNoOverlapCallbacks)
    {
        auto rig = PhysicsTestRig();
        auto trigger_entity = rig.world->create_entity("trigger");
        trigger_entity.add_component<tbx::Transform>();
        trigger_entity.add_component<tbx::BoxTrigger>().is_overlap_enabled = false;
        rig.create_collider_entity("other", 2.0F);

        auto physics = tbx::Physics(
            rig.backend,
            rig.asset_manager,
            std::weak_ptr<tbx::WorldManager>(),
            std::weak_ptr<tbx::ThreadManager>(),
            std::weak_ptr<tbx::JobSystem>(),
            rig.coordinator,
            tbx::PhysicsSettings {});

        auto callback_count = 0;
        trigger_entity.get_component<tbx::BoxTrigger>().overlap_begin_callbacks.push_back(
            [&callback_count](const tbx::ColliderOverlapEvent&) { ++callback_count; });

        physics.update(STEP_DELTA, tbx::PhysicsSettings {});
        physics.update(STEP_DELTA, tbx::PhysicsSettings {});

        EXPECT_EQ(callback_count, 0);
    }
}
