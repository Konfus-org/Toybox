#pragma once
#include "tbx/common/uuid.h"
#include "tbx/math/vectors.h"
#include "tbx/messages/message.h"
#include "tbx/tbx_api.h"

namespace tbx
{
    /// @brief
    /// Purpose: Represents the result payload returned by a physics raycast query.
    /// @details
    /// Ownership: Value type containing non-owning entity identifiers.
    /// Thread Safety: Safe for concurrent reads; synchronize external mutation.

    struct TBX_API RaycastResult
    {
        bool has_hit = false;
        Uuid hit_entity_id = {};
        Vec3 hit_position = Vec3(0.0F, 0.0F, 0.0F);
        float hit_fraction = 1.0F;

        operator bool() const
        {
            return has_hit && hit_entity_id.is_valid();
        }
    };

    /// @brief
    /// Purpose: Describes a single physics raycast query and provides helpers to execute it through
    /// the global dispatcher.
    /// @details
    /// Ownership: Value type that owns query parameters by copy.
    /// Thread Safety: Safe for concurrent reads; calling methods requires a valid thread-safe
    /// global dispatcher.

    struct TBX_API Raycast
    {
        Vec3 origin = Vec3(0.0F, 0.0F, 0.0F);
        Vec3 direction = Vec3(0.0F, 0.0F, -1.0F);
        float max_distance = 100.0F;
        bool ignore_entity = false;
        Uuid ignored_entity_id = {};

        /// @brief
        /// Purpose: Executes this raycast through the active global dispatcher.
        /// @details
        /// Ownership: Writes the result into the caller-provided output value.
        /// Thread Safety: Thread-safe if the global dispatcher implementation is thread-safe.

        bool try_cast(RaycastResult& out_result) const;

        /// @brief
        /// Purpose: Executes this raycast through the active global dispatcher and returns the
        /// resulting hit payload.
        /// @details
        /// Ownership: Returns the result by value.
        /// Thread Safety: Thread-safe if the global dispatcher implementation is thread-safe.

        RaycastResult cast() const;
    };

    /// @brief
    /// Purpose: Message request used to execute a physics raycast.
    /// @details
    /// Ownership: Owns a copy of the raycast query payload.
    /// Thread Safety: Safe to construct on any thread; handling depends on the dispatcher backend.

    struct TBX_API RaycastRequest : public Request<RaycastResult>
    {
        explicit RaycastRequest(const Raycast& raycast_query);
        ~RaycastRequest() noexcept override;

        Raycast raycast = {};
    };
}
