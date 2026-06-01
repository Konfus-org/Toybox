#include "jolt_physics_plugin.h"

namespace jolt_physics
{
    void JoltPhysicsPlugin::on_detach()
    {
        physics_backend = {};
    }
}
