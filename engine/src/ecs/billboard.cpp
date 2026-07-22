#include "tbx/ecs/billboard.h"
#include "tbx/ecs/sandbox.h"
#include "tbx/math/transform.h"

namespace tbx
{
    void update_billboards(Sandbox& sandbox, const Vec3& camera_position)
    {
        sandbox.each<Billboard>(
            [&](Toy toy, Billboard& billboard)
            {
                const Vec3 world_position =
                    Vec3(toy.get_world_transform() * Vec4(0.0f, 0.0f, 0.0f, 1.0f));
                auto to_camera = camera_position - world_position;
                if (billboard.lock_y)
                    to_camera.y = 0.0f; // upright: face the camera on the horizontal plane only
                if (length(to_camera) < 0.0001f)
                    return; // camera is on top of the toy — leave the current facing
                // The toy's -Z is its front (camera convention), so aim -Z at the camera. This
                // sets the LOCAL rotation; billboards are expected not to sit under a rotated
                // parent.
                toy.get_transform().rotation =
                    quat_look_at(normalize(to_camera), Vec3(0.0f, 1.0f, 0.0f));
            });
    }
}
