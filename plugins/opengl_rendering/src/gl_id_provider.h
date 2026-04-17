#pragma once
#include "tbx/common/typedefs.h"
#include "tbx/common/uuid.h"
#include "tbx/tbx_api.h"

namespace opengl_rendering
{
    /// @brief
    /// Purpose: Centralizes ID translation rules between TBX assets and OpenGL caches.
    /// @details
    /// Ownership: Stateless; does not own external resources.
    /// Thread Safety: Safe to call concurrently.
    class GlIdProvider final
    {
      public:
        /// @brief
        /// Purpose: Combines the provided identifiers into a stable cache key.
        /// @details
        /// Ownership: Returns a value type; the caller owns the copy.
        /// Thread Safety: Safe to call concurrently.
        tbx::Uuid provide(const tbx::Uuid& first, uint32 second) const;
    };
}
