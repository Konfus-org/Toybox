#include "tbx/async/cancellation_token.h"
#include <utility>

namespace tbx
{
    CancellationSource::CancellationSource()
        : _state(new ThreadSafe<bool>(false))
    {
    }

    CancellationToken CancellationSource::get_token() const
    {
        return CancellationToken(_state);
    }

    void CancellationSource::cancel() const
    {
        if (_state)
        {
            auto lock = _state->lock();
            lock.get() = true;
        }
    }

    bool CancellationSource::is_cancelled() const
    {
        if (!_state)
            return false;

        auto lock = _state->lock();
        return lock.get();
    }

    CancellationToken::CancellationToken(Ref<ThreadSafe<bool>> state)
        : _state(state)
    {
    }

    bool CancellationToken::is_cancelled() const
    {
        if (!_state)
            return false;

        auto lock = _state->lock();
        return lock.get();
    }

    CancellationToken::operator bool() const
    {
        return static_cast<bool>(_state);
    }
}
