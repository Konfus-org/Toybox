#pragma once
#include <atomic>
#include <memory>

namespace tbx
{
    class CancellationToken;

    /// \brief Produces cancellation tokens that share a single atomic flag.
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

    /// \brief Lightweight handle that observes cancellation requests from a CancellationSource.
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
