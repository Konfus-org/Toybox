#pragma once
#include "tbx/tbx_api.h"
#include "tbx/tsl/smart_pointers.h"
#include <atomic>

namespace tbx
{
    class CancellationToken;

    // Thread-safe source that owns the shared atomic flag backing associated tokens.
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

    // Lightweight, thread-safe observer for a CancellationSource-owned flag.
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
