#include "tbx/ecs/sandbox.h"
#include "tbx/ecs/billboard.h"
#include "tbx/gfx/camera.h"
#include "tbx/math/transform.h"
#include <optional>

namespace tbx
{
    // The world position of the first enabled camera, if any — what billboards turn to face.
    static std::optional<Vec3> primary_camera_position(Sandbox& sandbox)
    {
        auto position = std::optional<Vec3>();
        sandbox.each<Camera>(
            [&](Toy toy, Camera&)
            {
                if (position || !toy.is_enabled())
                    return;
                position = Vec3(toy.get_world_transform() * Vec4(0.0f, 0.0f, 0.0f, 1.0f));
            });
        return position;
    }

    // Turns every Billboard toy to face the given camera position. File-local: billboards are a
    // builtin component driven only through update_ecs.
    static void update_billboards(Sandbox& sandbox, const Vec3& camera_position)
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

    void update_ecs(
        Sandbox& sandbox,
        AssetsState& assets,
        EventsState& events,
        JobsState& jobs,
        const std::span<const Frustum> frustums)
    {
        // Builtin components settle first: billboards face the active camera (opt-in; nothing
        // rotates without a Billboard block) — after scripts/physics settled transforms, before
        // streaming/rendering.
        if (const auto camera_position = primary_camera_position(sandbox))
            update_billboards(sandbox, *camera_position);

        // Then the engine pulls streaming: every enabled camera contributed a frustum, and the
        // sandbox loads what any of them can see (also flushes a pending open()).
        stream(sandbox, assets, events, jobs, frustums);
    }
}
