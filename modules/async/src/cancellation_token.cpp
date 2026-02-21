#include "tbx/async/cancellation_token.h"
#include <memory>

namespace tbx
{
    CancellationSource::CancellationSource()
        : _state(std::make_shared<std::atomic_bool>(false))
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
            _state->store(true);
        }
    }

    bool CancellationSource::is_cancelled() const
    {
        if (!_state)
            return false;

        return _state->load();
    }

    CancellationToken::CancellationToken(std::shared_ptr<std::atomic_bool> state)
        : _state(state)
    {
    }

    bool CancellationToken::is_cancelled() const
    {
        if (!_state)
            return false;

        return _state->load();
    }

    CancellationToken::operator bool() const
    {
        return static_cast<bool>(_state);
    }
}
