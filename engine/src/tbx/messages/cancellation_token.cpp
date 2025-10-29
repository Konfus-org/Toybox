#include "tbx/messages/cancellation_token.h"
#include <atomic>
#include <memory>
#include <utility>

namespace tbx
{
    struct CancellationState
    {
        std::atomic<bool> cancelled{false};
    };

    CancellationSource::CancellationSource()
        : _state(std::make_shared<CancellationState>())
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
            _state->cancelled.store(true, std::memory_order_release);
        }
    }

    bool CancellationSource::is_cancelled() const
    {
        return _state && _state->cancelled.load(std::memory_order_acquire);
    }

    CancellationToken::CancellationToken(std::shared_ptr<CancellationState> state)
        : _state(std::move(state))
    {
    }

    bool CancellationToken::is_cancelled() const
    {
        return _state && _state->cancelled.load(std::memory_order_acquire);
    }
}
