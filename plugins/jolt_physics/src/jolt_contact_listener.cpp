#include "jolt_contact_listener.h"

namespace jolt_physics
{
    static std::uint32_t to_body_key(const JPH::BodyID& body_id)
    {
        return body_id.GetIndexAndSequenceNumber();
    }

    void JoltContactEventListener::set_contact(
        const JPH::Body& body_a,
        const JPH::Body& body_b,
        const JPH::ContactManifold& manifold)
    {
        // Sensor bodies report overlaps through the trigger path, never as contacts.
        if (body_a.IsSensor() || body_b.IsSensor())
            return;

        const JoltContactPoint point = JoltContactPoint {
            .position = manifold.mRelativeContactPointsOn1.empty()
                            ? manifold.mBaseOffset
                            : manifold.GetWorldSpaceContactPointOn1(0U),
            .normal = manifold.mWorldSpaceNormal,
        };
        const std::uint32_t key_a = to_body_key(body_a.GetID());
        const std::uint32_t key_b = to_body_key(body_b.GetID());

        std::lock_guard lock(_mutex);
        _contacts[key_a][key_b] = point;
        _contacts[key_b][key_a] = point;
    }

    void JoltContactEventListener::OnContactAdded(
        const JPH::Body& body_a,
        const JPH::Body& body_b,
        const JPH::ContactManifold& manifold,
        JPH::ContactSettings& /*settings*/)
    {
        set_contact(body_a, body_b, manifold);
    }

    void JoltContactEventListener::OnContactPersisted(
        const JPH::Body& body_a,
        const JPH::Body& body_b,
        const JPH::ContactManifold& manifold,
        JPH::ContactSettings& /*settings*/)
    {
        set_contact(body_a, body_b, manifold);
    }

    void JoltContactEventListener::OnContactRemoved(const JPH::SubShapeIDPair& sub_shape_pair)
    {
        const std::uint32_t key_a = to_body_key(sub_shape_pair.GetBody1ID());
        const std::uint32_t key_b = to_body_key(sub_shape_pair.GetBody2ID());

        std::lock_guard lock(_mutex);
        if (const auto a = _contacts.find(key_a); a != _contacts.end())
        {
            a->second.erase(key_b);
            if (a->second.empty())
                _contacts.erase(a);
        }
        if (const auto b = _contacts.find(key_b); b != _contacts.end())
        {
            b->second.erase(key_a);
            if (b->second.empty())
                _contacts.erase(b);
        }
    }

    void JoltContactEventListener::get_contacts(
        std::uint32_t body_key, std::vector<JoltContactNeighbor>& out_neighbors) const
    {
        std::lock_guard lock(_mutex);
        const auto it = _contacts.find(body_key);
        if (it == _contacts.end())
            return;

        out_neighbors.reserve(out_neighbors.size() + it->second.size());
        for (const auto& [other_key, point] : it->second)
        {
            out_neighbors.push_back(
                JoltContactNeighbor {
                    .other_body_key = other_key,
                    .position = point.position,
                    .normal = point.normal,
                });
        }
    }

    void JoltContactEventListener::remove_body(std::uint32_t body_key)
    {
        std::lock_guard lock(_mutex);
        const auto it = _contacts.find(body_key);
        if (it == _contacts.end())
            return;

        for (const auto& [other_key, point] : it->second)
        {
            if (const auto other = _contacts.find(other_key); other != _contacts.end())
            {
                other->second.erase(body_key);
                if (other->second.empty())
                    _contacts.erase(other);
            }
        }
        _contacts.erase(body_key);
    }

    void JoltContactEventListener::clear()
    {
        std::lock_guard lock(_mutex);
        _contacts.clear();
    }
}
