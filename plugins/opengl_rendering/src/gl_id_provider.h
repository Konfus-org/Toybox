#pragma once
#include "tbx/common/int.h"
#include "tbx/common/uuid.h"
#include "tbx/tbx_api.h"

namespace tbx::plugins
{
    /// <summary>Provides OpenGL cache keys for Toybox identifiers.</summary>
    /// <remarks>
    /// Purpose: Centralizes ID translation rules between TBX assets and OpenGL caches.
    /// Ownership: Stateless; does not own external resources.
    /// Thread Safety: Safe to call concurrently.
    /// </remarks>
    class GlIdProvider final
    {
      public:
        /// <summary>Provides a cache key for an identifier pair.</summary>
        /// <remarks>
        /// Purpose: Combines the provided identifiers into a stable cache key.
        /// Ownership: Returns a value type; the caller owns the copy.
        /// Thread Safety: Safe to call concurrently.
        /// </remarks>
        Uuid provide(const Uuid& first, uint32 second) const;
    };
}
