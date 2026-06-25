#include "tbx/types/components/collider.h"

namespace tbx
{
    void Trigger::request_overlap_scan()
    {
        is_manual_scan_requested = true;
    }

    BoxCollider::BoxCollider(Vec3 collider_half_extents)
        : half_extents(collider_half_extents)
    {
    }

    SphereCollider::SphereCollider(float collider_radius)
        : radius(collider_radius)
    {
    }

    CapsuleCollider::CapsuleCollider(float collider_radius, float collider_half_height)
        : radius(collider_radius)
        , half_height(collider_half_height)
    {
    }

    MeshCollider::MeshCollider(bool collider_is_convex)
        : is_convex(collider_is_convex)
    {
    }

    BoxTrigger::BoxTrigger(Vec3 trigger_half_extents)
        : half_extents(trigger_half_extents)
    {
    }

    SphereTrigger::SphereTrigger(float trigger_radius)
        : radius(trigger_radius)
    {
    }

    CapsuleTrigger::CapsuleTrigger(float trigger_radius, float trigger_half_height)
        : radius(trigger_radius)
        , half_height(trigger_half_height)
    {
    }

    MeshTrigger::MeshTrigger(bool trigger_is_convex)
        : is_convex(trigger_is_convex)
    {
    }
}
