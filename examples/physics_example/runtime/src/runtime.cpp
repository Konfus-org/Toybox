#include "runtime.h"
#include "tbx/app/application.h"
#include "tbx/graphics/camera.h"
#include "tbx/graphics/color.h"
#include "tbx/graphics/light.h"
#include "tbx/graphics/mesh.h"
#include "tbx/graphics/renderer.h"
#include "tbx/math/transform.h"
#include "tbx/math/trig.h"
#include "tbx/physics/collider.h"
#include "tbx/physics/physics.h"
#include <cmath>

namespace tbx::examples
{
    void PhysicsExampleRuntimePlugin::on_attach(IPluginHost& host)
    {
        auto& graphics = host.get_settings().graphics;
        graphics.shadow_map_resolution = 2048U;
        graphics.shadow_render_distance = 40.0F;
        graphics.shadow_softness = 1.1F;

        auto& ent_registry = host.get_entity_registry();

        auto sun = Entity("Light", ent_registry);
        sun.add_component<DirectionalLight>(RgbaColor::white, 0.5F, 0.15F);
        sun.add_component<Transform>(Vec3(0), to_radians(Vec3(-45.0F, 45.0F, 0.0F)), Vec3(1));

        auto camera = Entity("Camera", ent_registry);
        camera.add_component<Camera>();
        camera.add_component<Transform>(
            Vec3(0.0F, 3.0F, 14.0F),
            to_radians(Vec3(-8.0F, 0.0F, 0.0F)));

        auto ground_ent = Entity("Ground", ent_registry);
        ground_ent.add_component<Renderer>();
        ground_ent.add_component<DynamicMesh>(quad);
        ground_ent.add_component<MeshCollider>();
        ground_ent.add_component<Transform>(
            Vec3(0.0F, -2.5F, -5.0F),
            to_radians(Vec3(-90.0F, 0.0F, 0.0F)),
            Vec3(20.0F, 20.0F, 1.0F));

        auto falling_sphere = Entity("FallingSphere", ent_registry);
        falling_sphere.add_component<Renderer>();
        falling_sphere.add_component<DynamicMesh>(sphere);
        falling_sphere.add_component<Transform>(Vec3(0.0F, 6.0F, -5.2F));
        falling_sphere.add_component<SphereCollider>(0.5F);
        falling_sphere.add_component<Physics>();

        auto falling_cube = Entity("FallingCube", ent_registry);
        falling_cube.add_component<Renderer>();
        falling_cube.add_component<DynamicMesh>(cube);
        falling_cube.add_component<Transform>(Vec3(0.0F, 10.0F, -4.9F));
        falling_cube.add_component<CubeCollider>(Vec3(0.5F, 0.5F, 0.5F));
        falling_cube.add_component<Physics>();
    }
}
