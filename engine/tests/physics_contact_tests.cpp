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

    // Scripted physics backend: hands out handle-addressed bodies, remembers which body was created
    // at which x position, and reports whatever contacts/overlaps the test wired up through
    // get_state. Sensor bodies (built from a create_trigger shape) report every other body as an
    // overlap, mirroring how the real backend queries a trigger's occupants.
    class FakePhysicsBackend final : public tbx::IPhysicsBackend
    {
      public:
        void initialize(
            tbx::Vec3,
            uint32,
            uint32,
            uint32,
            uint32,
            uint32,
            float,
            float) override
        {
        }

        void shutdown() override
        {
        }

        void set_gravity(tbx::Vec3) override
        {
        }

        void set_solver_velocity_iterations(uint32) override
        {
        }

        void set_solver_position_iterations(uint32) override
        {
        }

        void set_max_linear_velocity(float) override
        {
        }

        void set_max_angular_velocity(float) override
        {
        }

        void step(const tbx::DeltaTime&) override
        {
        }

        bool get_state(const tbx::PhysicsHandle& handle, tbx::PhysicsEntityState& out_state) override
        {
            out_state = {};
            const auto body_it = bodies.find(handle.id);
            if (body_it == bodies.end())
                return false;

            if (const auto contact_it = contacts_by_body.find(handle.id);
                contact_it != contacts_by_body.end())
                out_state.contacts = contact_it->second;

            if (body_it->second.is_sensor)
            {
                for (const auto& other : created_bodies)
                {
                    if (!(other == handle))
                        out_state.overlaps.push_back(other);
                }
            }
            return true;
        }

        bool raycast(
            const tbx::RaycastQuery&,
            const std::vector<tbx::PhysicsHandle>&,
            tbx::PhysicsRaycastHit&) const override
        {
            return false;
        }

        tbx::PhysicsHandle create_collider(const tbx::Collider&, const tbx::Mesh&) override
        {
            pending_is_sensor = false;
            return tbx::PhysicsHandle(tbx::Uuid::generate());
        }

        tbx::PhysicsHandle create_trigger(const tbx::Trigger&, const tbx::Mesh&) override
        {
            pending_is_sensor = true;
            return tbx::PhysicsHandle(tbx::Uuid::generate());
        }

        tbx::PhysicsHandle create_rigidbody(tbx::Transform transform, tbx::Rigidbody) override
        {
            const auto handle = tbx::PhysicsHandle(tbx::Uuid::generate());
            bodies[handle.id] = BodyInfo {.handle = handle, .is_sensor = pending_is_sensor};
            created_bodies.push_back(handle);
            body_by_position_x[transform.position.x] = handle;
            pending_is_sensor = false;
            return handle;
        }

        void destroy(const tbx::PhysicsHandle& handle) override
        {
            bodies.erase(handle.id);
            contacts_by_body.erase(handle.id);
            std::erase_if(
                created_bodies, [&handle](const tbx::PhysicsHandle& other) { return other == handle; });
        }

        std::vector<tbx::Vec3>& get_debug_shape(tbx::PhysicsHandle) const override
        {
            return debug_shape;
        }

      public:
        // Wires a symmetric solid contact between two bodies so get_state reports each from the
        // other's perspective.
        void add_contact(
            const tbx::PhysicsHandle& body_a,
            const tbx::PhysicsHandle& body_b,
            const tbx::Vec3& position = tbx::Vec3(0.0F),
            const tbx::Vec3& normal = tbx::Vec3(0.0F))
        {
            contacts_by_body[body_a.id].push_back(
                tbx::PhysicsContact {.other = body_b, .position = position, .normal = normal});
            contacts_by_body[body_b.id].push_back(
                tbx::PhysicsContact {.other = body_a, .position = position, .normal = normal});
        }

        struct BodyInfo
        {
            tbx::PhysicsHandle handle = {};
            bool is_sensor = false;
        };

        std::unordered_map<tbx::Uuid, BodyInfo> bodies = {};
        std::unordered_map<tbx::Uuid, std::vector<tbx::PhysicsContact>> contacts_by_body = {};
        std::vector<tbx::PhysicsHandle> created_bodies = {};
        std::unordered_map<float, tbx::PhysicsHandle> body_by_position_x = {};
        bool pending_is_sensor = false;
        mutable std::vector<tbx::Vec3> debug_shape = {};
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

    tbx::Physics make_physics(PhysicsTestRig& rig, bool with_coordinator)
    {
        if (with_coordinator)
            return tbx::Physics(
                rig.backend,
                rig.asset_manager,
                std::weak_ptr<tbx::WorldManager>(),
                std::weak_ptr<tbx::ThreadManager>(),
                rig.coordinator,
                tbx::PhysicsSettings {});

        return tbx::Physics(
            rig.backend,
            rig.asset_manager,
            std::weak_ptr<tbx::WorldManager>(),
            std::weak_ptr<tbx::ThreadManager>(),
            tbx::PhysicsSettings {});
    }

    TEST(PhysicsContactTests, BeginContactInvokesCallbacksOnBothEntities)
    {
        auto rig = PhysicsTestRig();
        auto entity_a = rig.create_collider_entity("a", 1.0F);
        auto entity_b = rig.create_collider_entity("b", 2.0F);

        auto physics = make_physics(rig, true);

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
        rig.backend->add_contact(
            rig.backend->body_by_position_x.at(1.0F),
            rig.backend->body_by_position_x.at(2.0F),
            tbx::Vec3(0.5F, 1.0F, 2.0F),
            tbx::Vec3(0.0F, 1.0F, 0.0F));

        // Second update commits the step results and diffs the new contact into begin callbacks.
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

        auto physics = make_physics(rig, true);

        auto callback_count = 0;
        entity_a.get_component<tbx::BoxCollider>().contact_begin_callbacks.push_back(
            [&callback_count](const tbx::ColliderContactEvent&) { ++callback_count; });

        physics.update(STEP_DELTA, tbx::PhysicsSettings {});
        // The other body is not tracked by Physics (e.g. destroyed between the step and the read),
        // so the contact must be dropped gracefully.
        rig.backend->contacts_by_body[rig.backend->body_by_position_x.at(1.0F).id].push_back(
            tbx::PhysicsContact {.other = tbx::PhysicsHandle(tbx::Uuid::generate())});

        physics.update(STEP_DELTA, tbx::PhysicsSettings {});

        EXPECT_EQ(callback_count, 0);
    }

    TEST(PhysicsContactTests, EndContactInvokesEndCallbacksWhenContactClears)
    {
        auto rig = PhysicsTestRig();
        auto entity_a = rig.create_collider_entity("a", 1.0F);
        auto entity_b = rig.create_collider_entity("b", 2.0F);

        auto physics = make_physics(rig, true);

        auto begin_count = 0;
        auto end_events = std::vector<tbx::ColliderContactEvent>();
        auto& collider = entity_a.get_component<tbx::BoxCollider>();
        collider.contact_begin_callbacks.push_back(
            [&begin_count](const tbx::ColliderContactEvent&) { ++begin_count; });
        collider.contact_end_callbacks.push_back(
            [&end_events](const tbx::ColliderContactEvent& event) { end_events.push_back(event); });

        physics.update(STEP_DELTA, tbx::PhysicsSettings {});
        rig.backend->add_contact(
            rig.backend->body_by_position_x.at(1.0F), rig.backend->body_by_position_x.at(2.0F));

        // The contact appears (begin), then is cleared so the next read diffs it into an end.
        physics.update(STEP_DELTA, tbx::PhysicsSettings {});
        rig.backend->contacts_by_body.clear();
        physics.update(STEP_DELTA, tbx::PhysicsSettings {});

        EXPECT_EQ(begin_count, 1);
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
        auto physics = make_physics(rig, false);

        auto callback_count = 0;
        entity_a.get_component<tbx::BoxCollider>().contact_begin_callbacks.push_back(
            [&callback_count](const tbx::ColliderContactEvent&) { ++callback_count; });

        physics.update(STEP_DELTA, tbx::PhysicsSettings {});
        rig.backend->add_contact(
            rig.backend->body_by_position_x.at(1.0F), rig.backend->body_by_position_x.at(2.0F));

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

        auto physics = make_physics(rig, true);

        auto begin_events = std::vector<tbx::ColliderOverlapEvent>();
        auto stay_events = std::vector<tbx::ColliderOverlapEvent>();
        auto& trigger = trigger_entity.get_component<tbx::BoxTrigger>();
        trigger.overlap_begin_callbacks.push_back(
            [&begin_events](const tbx::ColliderOverlapEvent& event)
            { begin_events.push_back(event); });
        trigger.overlap_stay_callbacks.push_back(
            [&stay_events](const tbx::ColliderOverlapEvent& event)
            { stay_events.push_back(event); });

        // The fake backend reports every other body as overlapping a sensor, so the second update
        // (the one that commits results) sees a fresh overlap and the third sees it persist.
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

        auto physics = make_physics(rig, true);

        auto callback_count = 0;
        trigger_entity.get_component<tbx::BoxTrigger>().overlap_begin_callbacks.push_back(
            [&callback_count](const tbx::ColliderOverlapEvent&) { ++callback_count; });

        physics.update(STEP_DELTA, tbx::PhysicsSettings {});
        physics.update(STEP_DELTA, tbx::PhysicsSettings {});

        EXPECT_EQ(callback_count, 0);
    }
}
