#pragma once
#include <Jolt/Jolt.h>
#include <cstdint>

// clang-format off
#include <Jolt/Physics/Body/Body.h>
#include <Jolt/Physics/Collision/ContactListener.h>
// clang-format on

#include <mutex>
#include <unordered_map>
#include <vector>

namespace jolt_physics
{
    /// @brief
    /// Purpose: One body a queried body is currently touching, handed back to the backend so it can
    /// fill a PhysicsEntityState's contact list.
    struct JoltContactNeighbor
    {
        std::uint32_t other_body_key = 0U;
        JPH::RVec3 position = JPH::RVec3::sZero();
        JPH::Vec3 normal = JPH::Vec3::sZero();
    };

    /// @brief
    /// Purpose: Tracks the set of solid body pairs currently in contact so the backend can report
    /// each body's live contacts through get_state (the engine diffs them into begin/end callbacks).
    /// @details
    /// Ownership: Owns the adjacency of current contacts.
    /// Thread Safety: Jolt invokes the contact callbacks from its worker threads during Update, so
    /// the adjacency is mutex-guarded. Reads happen once the step has been joined.
    class JoltContactEventListener final : public JPH::ContactListener
    {
      public:
        /// @brief Records a new solid contact pair. Sensor (trigger) bodies are skipped — they report
        /// through the trigger overlap path instead.
        void OnContactAdded(
            const JPH::Body& body_a,
            const JPH::Body& body_b,
            const JPH::ContactManifold& manifold,
            JPH::ContactSettings& settings) override;

        /// @brief Refreshes the contact point of an existing solid pair.
        void OnContactPersisted(
            const JPH::Body& body_a,
            const JPH::Body& body_b,
            const JPH::ContactManifold& manifold,
            JPH::ContactSettings& settings) override;

        /// @brief Drops a contact pair that stopped touching.
        void OnContactRemoved(const JPH::SubShapeIDPair& sub_shape_pair) override;

        /// @brief Appends the bodies currently touching `body_key` into `out_neighbors`.
        void get_contacts(std::uint32_t body_key, std::vector<JoltContactNeighbor>& out_neighbors)
            const;

        /// @brief Forgets every contact involving a destroyed body so stale pairs never surface.
        void remove_body(std::uint32_t body_key);

        /// @brief Drops all tracked contacts.
        void clear();

      private:
        struct JoltContactPoint
        {
            JPH::RVec3 position = JPH::RVec3::sZero();
            JPH::Vec3 normal = JPH::Vec3::sZero();
        };

        void set_contact(
            const JPH::Body& body_a,
            const JPH::Body& body_b,
            const JPH::ContactManifold& manifold);

        mutable std::mutex _mutex = {};
        // body key -> (other body key -> contact point)
        std::unordered_map<std::uint32_t, std::unordered_map<std::uint32_t, JoltContactPoint>>
            _contacts = {};
    };
}
