#include "tbx/async/cancellation_token.h"
#include <utility>

namespace tbx
{
    CancellationSource::CancellationSource()
        : _state(Ref<bool>(false))
    {
    }

    CancellationToken CancellationSource::get_token() const
    {
        auto lock = _state.lock();
        return CancellationToken(lock.get());
    }

    void CancellationSource::cancel() const
    {
        auto lock = _state.lock();
        auto& flag = lock.get();
        if (flag)
            *flag = true;
    }

    bool CancellationSource::is_cancelled() const
    {
        auto lock = _state.lock();
        auto& flag = lock.get();
        return flag && *flag;
    }

    CancellationToken::CancellationToken(Ref<bool> state)
        : _state(state)
    {
    }

    bool CancellationToken::is_cancelled() const
    {
        return _state && *_state;
    }

    CancellationToken::operator bool() const
    {
        return static_cast<bool>(_state);
    }
}
