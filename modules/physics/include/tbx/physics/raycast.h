#pragma once
#include "tbx/common/uuid.h"
#include "tbx/math/vectors.h"
#include "tbx/messages/message.h"
#include "tbx/tbx_api.h"

namespace tbx
{
    /// <summary>
    /// Purpose: Represents the result payload returned by a physics raycast query.
    /// </summary>
    /// <remarks>
    /// Ownership: Value type containing non-owning entity identifiers.
    /// Thread Safety: Safe for concurrent reads; synchronize external mutation.
    /// </remarks>
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

    /// <summary>
    /// Purpose: Describes a single physics raycast query and provides helpers to execute it through
    /// the global dispatcher.
    /// </summary>
    /// <remarks>
    /// Ownership: Value type that owns query parameters by copy.
    /// Thread Safety: Safe for concurrent reads; calling methods requires a valid thread-safe
    /// global dispatcher.
    /// </remarks>
    struct TBX_API Raycast
    {
        Vec3 origin = Vec3(0.0F, 0.0F, 0.0F);
        Vec3 direction = Vec3(0.0F, 0.0F, -1.0F);
        float max_distance = 100.0F;
        bool ignore_entity = false;
        Uuid ignored_entity_id = {};

        /// <summary>
        /// Purpose: Executes this raycast through the active global dispatcher.
        /// </summary>
        /// <remarks>
        /// Ownership: Writes the result into the caller-provided output value.
        /// Thread Safety: Thread-safe if the global dispatcher implementation is thread-safe.
        /// </remarks>
        bool try_cast(RaycastResult& out_result) const;

        /// <summary>
        /// Purpose: Executes this raycast through the active global dispatcher and returns the
        /// resulting hit payload.
        /// </summary>
        /// <remarks>
        /// Ownership: Returns the result by value.
        /// Thread Safety: Thread-safe if the global dispatcher implementation is thread-safe.
        /// </remarks>
        RaycastResult cast() const;
    };

    /// <summary>
    /// Purpose: Message request used to execute a physics raycast.
    /// </summary>
    /// <remarks>
    /// Ownership: Owns a copy of the raycast query payload.
    /// Thread Safety: Safe to construct on any thread; handling depends on the dispatcher backend.
    /// </remarks>
    struct TBX_API RaycastRequest : public Request<RaycastResult>
    {
        /// <summary>
        /// Purpose: Creates a request from an existing raycast query payload.
        /// </summary>
        /// <remarks>
        /// Ownership: Copies the query payload into the request.
        /// Thread Safety: Safe to call on any thread before dispatch.
        /// </remarks>
        explicit RaycastRequest(const Raycast& raycast_query);

        /// <summary>
        /// Purpose: Destroys the request instance.
        /// </summary>
        /// <remarks>
        /// Ownership: Releases only request-owned memory.
        /// Thread Safety: Safe to destroy when no concurrent access occurs.
        /// </remarks>
        ~RaycastRequest() noexcept override;

        Raycast raycast = {};
    };
}
