#pragma once
#include "tbx/async/lock.h"
#include "tbx/common/smart_pointers.h"
#include "tbx/tbx_api.h"

namespace tbx
{
    class CancellationToken;

    class TBX_API CancellationSource
    {
      public:
        CancellationSource();

        CancellationToken get_token() const;
        void cancel() const;
        bool is_cancelled() const;

      private:
        Ref<ThreadSafe<bool>> _state;
    };

    class TBX_API CancellationToken
    {
      public:
        CancellationToken() = default;
        CancellationToken(Ref<ThreadSafe<bool>> state);
        bool is_cancelled() const;
        operator bool() const;

      private:
        Ref<ThreadSafe<bool>> _state;
    };
}
