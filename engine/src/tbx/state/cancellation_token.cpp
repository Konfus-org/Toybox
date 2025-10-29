#include "tbx/state/cancellation_token.h"
#include <utility>

namespace tbx
{
    CancellationSource::CancellationSource()
        : _state(make_ref<std::atomic<bool>>(false))
    {
    }

    CancellationToken CancellationSource::token() const
    {
        return CancellationToken(_state);
    }

    void CancellationSource::cancel() const
    {
        if (_state)
        {
            _state->store(true, std::memory_order_release);
        }
    }

    bool CancellationSource::is_cancelled() const
    {
        return _state && _state->load(std::memory_order_acquire);
    }

    CancellationToken::CancellationToken(Ref<std::atomic<bool>> state)
        : _state(std::move(state))
    {
    }

    bool CancellationToken::is_cancelled() const
    {
        return _state && _state->load(std::memory_order_acquire);
    }
}
