#pragma once
#include <memory>

namespace tbx
{
    struct CancellationState;
    class CancellationToken;

    class CancellationSource
    {
    public:
        CancellationSource();

        CancellationToken token() const;
        void cancel() const;
        bool is_cancelled() const;

    private:
        std::shared_ptr<CancellationState> _state;
    };

    class CancellationToken
    {
    public:
        CancellationToken() = default;

        bool is_cancelled() const;
        explicit operator bool() const { return static_cast<bool>(_state); }

    private:
        explicit CancellationToken(std::shared_ptr<CancellationState> state);

        std::shared_ptr<CancellationState> _state;

        friend class CancellationSource;
    };
}
