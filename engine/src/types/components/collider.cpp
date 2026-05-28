#include "tbx/types/components/collider.h"

namespace tbx
{
    MeshCollider::MeshCollider(bool collider_is_convex, ColliderTrigger collider_trigger)
        : is_convex(collider_is_convex)
        , trigger(std::move(collider_trigger))
    {
    }

    CubeCollider::CubeCollider(Vec3 collider_half_extents, ColliderTrigger collider_trigger)
        : half_extents(collider_half_extents)
        , trigger(std::move(collider_trigger))
    {
    }

    SphereCollider::SphereCollider(float collider_radius, ColliderTrigger collider_trigger)
        : radius(collider_radius)
        , trigger(std::move(collider_trigger))
    {
    }

    CapsuleCollider::CapsuleCollider(
        float collider_radius,
        float collider_half_height,
        ColliderTrigger collider_trigger)
        : radius(collider_radius)
        , half_height(collider_half_height)
        , trigger(std::move(collider_trigger))
    {
    }

    void ColliderTrigger::request_overlap_scan()
    {
        is_manual_scan_requested = true;
    }
}
