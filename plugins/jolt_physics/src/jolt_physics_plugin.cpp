#include "jolt_physics_plugin.h"

namespace jolt_physics
{
    void JoltPhysics::on_detach()
    {
        physics_backend = {};
    }
}
