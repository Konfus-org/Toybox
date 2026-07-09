#include "jolt_contact_listener.h"

namespace jolt_physics
{
    void JoltContactEventListener::OnContactAdded(
        const JPH::Body& body_a,
        const JPH::Body& body_b,
        const JPH::ContactManifold& manifold,
        JPH::ContactSettings& /*settings*/)
    {
        // Sensor bodies report overlaps through the trigger path, never as contact events.
        if (body_a.IsSensor() || body_b.IsSensor())
            return;

        const auto record = JoltContactRecord {
            .body_a = body_a.GetID(),
            .body_b = body_b.GetID(),
            .position = manifold.mRelativeContactPointsOn1.empty()
                            ? manifold.mBaseOffset
                            : manifold.GetWorldSpaceContactPointOn1(0U),
            .normal = manifold.mWorldSpaceNormal,
            .phase = tbx::PhysicsContactPhase::BEGIN,
        };

        std::lock_guard lock(_mutex);
        _records.push_back(record);
    }

    void JoltContactEventListener::OnContactRemoved(const JPH::SubShapeIDPair& sub_shape_pair)
    {
        // Only the body pair is known at removal; position/normal stay zero. Sensor filtering
        // happens at drain time because the bodies are not accessible here.
        const auto record = JoltContactRecord {
            .body_a = sub_shape_pair.GetBody1ID(),
            .body_b = sub_shape_pair.GetBody2ID(),
            .phase = tbx::PhysicsContactPhase::END,
        };

        std::lock_guard lock(_mutex);
        _records.push_back(record);
    }

    void JoltContactEventListener::drain(std::vector<JoltContactRecord>& out_records)
    {
        std::lock_guard lock(_mutex);
        out_records.insert(out_records.end(), _records.begin(), _records.end());
        _records.clear();
    }
}
