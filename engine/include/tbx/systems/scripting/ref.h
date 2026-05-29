#pragma once
#include "tbx/systems/debugging/macros.h"

namespace tbx
{
    /// @brief
    /// Purpose: Common pointer-like contract for non-owning runtime references.
    /// @details
    /// Ownership: Does not own the referenced object. Returned pointers are only valid for the
    /// current runtime state and should not be cached across frames.
    template <typename TValue>
    class IRef
    {
      public:
        virtual ~IRef() noexcept = default;

      public:
        TValue& get() const
        {
            auto* resolved = try_get();
            TBX_ASSERT(resolved != nullptr, "Reference target is not resolved.");
            return *resolved;
        }

        bool is_resolved() const
        {
            return try_get() != nullptr;
        }

        virtual TValue* try_get() const = 0;
    };
}
