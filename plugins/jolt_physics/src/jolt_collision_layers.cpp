#include "jolt_collision_layers.h"
#include "internal/jolt_collision_layers_internal.h"
#include <cstdint>
namespace jolt_physics
{
    JPH::ObjectLayer get_static_object_layer()
    {
        return internal::StaticObjectLayer;
    }

    JPH::ObjectLayer get_moving_object_layer()
    {
        return internal::MovingObjectLayer;
    }

    const JPH::BroadPhaseLayerInterface& get_broad_phase_layer_interface()
    {
        static const auto interface_instance = internal::PhysicsBroadPhaseLayerInterface();
        return interface_instance;
    }

    const JPH::ObjectVsBroadPhaseLayerFilter& get_object_vs_broad_phase_layer_filter()
    {
        static const auto filter_instance = internal::PhysicsObjectVsBroadPhaseLayerFilter();
        return filter_instance;
    }

    const JPH::ObjectLayerPairFilter& get_object_layer_pair_filter()
    {
        static const auto filter_instance = internal::PhysicsObjectLayerPairFilter();
        return filter_instance;
    }
}
