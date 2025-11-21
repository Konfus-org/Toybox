#pragma once
#include "tbx/common/smart_pointers.h"
#include "tbx/tbx_api.h"
#include <atomic>

namespace tbx
{
    // Creates and manages cancellation tokens backed by a shared atomic flag.
    // Purpose: Allows producers to propagate cancellation to consumers listening via tokens.
    // Ownership: CancellationSource owns the shared flag and hands out lightweight token views
    // that share it; callers must ensure tokens are discarded before the source is destroyed.
    // Thread-safety: All member functions are thread-safe; multiple threads may request tokens
    // and cancel them concurrently.
    class TBX_API CancellationSource
    {
      public:
        CancellationSource();

        CancellationToken get_token() const;
        void cancel() const;
        bool is_cancelled() const;

      private:
        Ref<std::atomic<bool>> _state;
    };

    // Lightweight, thread-safe observer that reports whether its originating source cancelled.
    // Purpose: Enables consumers to check for cancellation signaled by a paired source.
    // Ownership: Non-owning view of a CancellationSource-managed flag; remains valid while the
    // source outlives the token.
    // Thread-safety: All member functions are thread-safe and may be called concurrently.
    class TBX_API CancellationToken
    {
      public:
        CancellationToken() = default;

        bool is_cancelled() const;
        operator bool() const
        {
            return static_cast<bool>(_state);
        }

      private:
        CancellationToken(Ref<std::atomic<bool>> state);

        Ref<std::atomic<bool>> _state;

        friend class CancellationSource;
    };
}
