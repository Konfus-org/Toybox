#include "tbx/core/systems/math/quaternions.h"

namespace tbx
{
    Quat normalize(Quat q)
    {
        return glm::normalize(q);
    }
}
