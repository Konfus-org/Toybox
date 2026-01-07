#pragma once
#include "tbx/tbx_api.h"
#include <atomic>
#include <memory>

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
        std::shared_ptr<std::atomic_bool> _state;
    };

    class TBX_API CancellationToken
    {
      public:
        CancellationToken() = default;
        CancellationToken(std::shared_ptr<std::atomic_bool> state);
        bool is_cancelled() const;
        operator bool() const;

      private:
        std::shared_ptr<std::atomic_bool> _state;
    };
}
