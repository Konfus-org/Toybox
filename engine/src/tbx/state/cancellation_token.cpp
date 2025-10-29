#include "tbx/state/cancellation_token.h"
#include <memory>
#include <utility>

namespace tbx
{
    CancellationSource::CancellationSource()
        : _state(std::make_shared<std::atomic<bool>>(false))
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

    CancellationToken::CancellationToken(std::shared_ptr<std::atomic<bool>> state)
        : _state(std::move(state))
    {
    }

    bool CancellationToken::is_cancelled() const
    {
        return _state && _state->load(std::memory_order_acquire);
    }
}
