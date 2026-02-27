#include "tbx/examples/room.h"
#include "tbx/graphics/mesh.h"
#include "tbx/graphics/renderer.h"
#include "tbx/math/transform.h"
#include "tbx/math/trig.h"
#include "tbx/physics/collider.h"

namespace example_common
{
    using namespace tbx;
    void Room::create(EntityRegistry& entity_registry, const RoomSettings& settings)
    {
        destroy();

        _ground = Entity("Ground", entity_registry);
        _ground.add_component<Renderer>();
        _ground.add_component<DynamicMesh>(quad);
        if (settings.include_colliders)
            _ground.add_component<MeshCollider>();
        _ground.add_component<Transform>(
            settings.center,
            to_radians(Vec3(-90.0F, 0.0F, 0.0F)),
            Vec3(40.0F, 40.0F, 1.0F));

        _front_wall = Entity("Wall_F", entity_registry);
        _front_wall.add_component<Renderer>();
        _front_wall.add_component<DynamicMesh>(cube);
        if (settings.include_colliders)
            _front_wall.add_component<CubeCollider>(Vec3(22.5F, 5.5F, 0.75F));
        _front_wall.add_component<Transform>(
            settings.center + Vec3(0.0F, 5.0F, -20.0F),
            Quat(Vec3(0.0F, 0.0F, 0.0F)),
            Vec3(45.0F, 11.0F, 1.5F));

        _left_wall = Entity("Wall_L", entity_registry);
        _left_wall.add_component<Renderer>();
        _left_wall.add_component<DynamicMesh>(cube);
        if (settings.include_colliders)
            _left_wall.add_component<CubeCollider>(Vec3(0.75F, 5.5F, 22.5F));
        _left_wall.add_component<Transform>(
            settings.center + Vec3(-20.0F, 5.0F, 0.0F),
            Quat(Vec3(0.0F, 0.0F, 0.0F)),
            Vec3(1.5F, 11.0F, 45.0F));

        _right_wall = Entity("Wall_R", entity_registry);
        _right_wall.add_component<Renderer>();
        _right_wall.add_component<DynamicMesh>(cube);
        if (settings.include_colliders)
            _right_wall.add_component<CubeCollider>(Vec3(0.75F, 5.5F, 22.5F));
        _right_wall.add_component<Transform>(
            settings.center + Vec3(20.0F, 5.0F, 0.0F),
            Quat(Vec3(0.0F, 0.0F, 0.0F)),
            Vec3(1.5F, 11.0F, 45.0F));

        _back_wall = Entity("Wall_B", entity_registry);
        _back_wall.add_component<Renderer>();
        _back_wall.add_component<DynamicMesh>(cube);
        if (settings.include_colliders)
            _back_wall.add_component<CubeCollider>(Vec3(22.5F, 5.5F, 0.75F));
        _back_wall.add_component<Transform>(
            settings.center + Vec3(0.0F, 5.0F, 20.0F),
            Quat(Vec3(0.0F, 0.0F, 0.0F)),
            Vec3(45.0F, 11.0F, 1.5F));
    }

    void Room::destroy()
    {
        _ground.destroy();
        _front_wall.destroy();
        _left_wall.destroy();
        _right_wall.destroy();
        _back_wall.destroy();

        _ground = {};
        _front_wall = {};
        _left_wall = {};
        _right_wall = {};
        _back_wall = {};
    }
}
