#pragma once
#include <atomic>
#include <memory>

namespace tbx
{
    namespace detail
    {
        struct CancellationState
        {
            std::atomic<bool> cancelled{false};
        };
    }

    class CancellationToken;

    class CancellationSource
    {
    public:
        CancellationSource();

        CancellationToken token() const;
        void cancel() const;
        bool is_cancelled() const;

    private:
        std::shared_ptr<detail::CancellationState> _state;
    };

    class CancellationToken
    {
    public:
        CancellationToken() = default;

        bool is_cancelled() const
        {
            return _state && _state->cancelled.load(std::memory_order_acquire);
        }

        explicit operator bool() const { return static_cast<bool>(_state); }

    private:
        explicit CancellationToken(std::shared_ptr<detail::CancellationState> state);

        std::shared_ptr<detail::CancellationState> _state;

        friend class CancellationSource;
    };
}

inline tbx::CancellationSource::CancellationSource()
    : _state(std::make_shared<detail::CancellationState>())
{
}

inline tbx::CancellationToken tbx::CancellationSource::token() const
{
    return CancellationToken(_state);
}

inline void tbx::CancellationSource::cancel() const
{
    if (_state)
    {
        _state->cancelled.store(true, std::memory_order_release);
    }
}

inline bool tbx::CancellationSource::is_cancelled() const
{
    return _state && _state->cancelled.load(std::memory_order_acquire);
}

inline tbx::CancellationToken::CancellationToken(std::shared_ptr<detail::CancellationState> state)
    : _state(std::move(state))
{
}
