#pragma once
#include <atomic>
#include <memory>

namespace tbx
{
    class CancellationToken;

    // Thread-safe source that owns the shared atomic flag backing associated tokens.
    class CancellationSource
    {
    public:
        CancellationSource();

        CancellationToken token() const;
        void cancel() const;
        bool is_cancelled() const;

    private:
        std::shared_ptr<std::atomic<bool>> _state;
    };

    // Lightweight, thread-safe observer for a CancellationSource-owned flag.
    class CancellationToken
    {
    public:
        CancellationToken() = default;

        bool is_cancelled() const;
        explicit operator bool() const { return static_cast<bool>(_state); }

    private:
        explicit CancellationToken(std::shared_ptr<std::atomic<bool>> state);

        std::shared_ptr<std::atomic<bool>> _state;

        friend class CancellationSource;
    };
}
