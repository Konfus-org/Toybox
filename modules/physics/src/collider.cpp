#include "tbx/physics/collider.h"

namespace tbx
{
    ColliderType Collider::get_type() const
    {
        if (std::holds_alternative<MeshCollider>(shape))
            return ColliderType::Mesh;

        if (std::holds_alternative<CubeCollider>(shape))
            return ColliderType::Cube;

        if (std::holds_alternative<SphereCollider>(shape))
            return ColliderType::Sphere;

        return ColliderType::Capsule;
    }
}
