#pragma once
#include "tbx/tbx_api.h"
#include "tbx/std/smart_pointers.h"
#include <atomic>

namespace tbx
{
    class CancellationToken;

    // Creates and manages cancellation tokens backed by a shared atomic flag.
    // Ownership: Owns the shared flag and returns lightweight token copies referencing it.
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
    // Ownership: Non-owning view of a CancellationSource-managed flag.
    // Thread-safety: All member functions are thread-safe and may be called concurrently.
    class TBX_API CancellationToken
    {
      public:
        CancellationToken() = default;

        bool is_cancelled() const;
        explicit operator bool() const
        {
            return static_cast<bool>(_state);
        }

      private:
        explicit CancellationToken(tbx::Ref<std::atomic<bool>> state);

        tbx::Ref<std::atomic<bool>> _state;

        friend class CancellationSource;
    };
}
