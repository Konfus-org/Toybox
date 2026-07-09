#pragma once
#include "tbx/interfaces/physics_backend.h"
#include <Jolt/Jolt.h>

// clang-format off
#include <Jolt/Physics/Body/Body.h>
#include <Jolt/Physics/Collision/ContactListener.h>
// clang-format on

#include <mutex>
#include <vector>

namespace jolt_physics
{
    /// @brief
    /// Purpose: One buffered contact transition captured during a simulation step, in raw Jolt
    /// terms. END records carry zero position/normal — only the body pair is known at removal.
    /// @details
    /// Ownership: Value type owned by the listener buffer until drained.
    /// Thread Safety: Safe to copy between threads.
    struct JoltContactRecord
    {
        JPH::BodyID body_a = {};
        JPH::BodyID body_b = {};
        JPH::RVec3 position = JPH::RVec3::sZero();
        JPH::Vec3 normal = JPH::Vec3::sZero();
        tbx::PhysicsContactPhase phase = tbx::PhysicsContactPhase::BEGIN;
    };

    /// @brief
    /// Purpose: Buffers Jolt contact begin/end callbacks so the backend can hand them to the engine
    /// once the simulation step has finished.
    /// @details
    /// Ownership: Owns the pending record buffer.
    /// Thread Safety: Jolt invokes the contact callbacks from its worker threads during Update, so
    /// the buffer is mutex-guarded. `drain` must only be called while no step is in flight.
    class JoltContactEventListener final : public JPH::ContactListener
    {
      public:
        /// @brief
        /// Purpose: Buffers a BEGIN record for a new contact pair. Sensor (trigger) bodies are
        /// skipped — they report through the trigger overlap path instead.
        void OnContactAdded(
            const JPH::Body& body_a,
            const JPH::Body& body_b,
            const JPH::ContactManifold& manifold,
            JPH::ContactSettings& settings) override;

        /// @brief
        /// Purpose: Buffers an END record for a contact pair that stopped touching.
        void OnContactRemoved(const JPH::SubShapeIDPair& sub_shape_pair) override;

        /// @brief
        /// Purpose: Moves all records buffered since the last drain into `out_records` (appending)
        /// and clears the internal buffer.
        /// @details
        /// Ownership: Appends value records into caller-owned storage.
        /// Thread Safety: Must only be called while no simulation step is in flight.
        void drain(std::vector<JoltContactRecord>& out_records);

      private:
        std::mutex _mutex = {};
        std::vector<JoltContactRecord> _records = {};
    };
}
