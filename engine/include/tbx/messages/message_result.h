#pragma once
#include <memory>

namespace tbx
{
    enum class MessageStatus
    {
        InProgress,
        Handled,
        Processed,
        Cancelled,
        Failed
    };

    class MessageCoordinator;

    class MessageResult
    {
    public:
        MessageResult();

        MessageStatus status() const;
        bool is_in_progress() const { return status() == MessageStatus::InProgress; }
        bool is_cancelled() const { return status() == MessageStatus::Cancelled; }
        bool is_failed() const { return status() == MessageStatus::Failed; }
        bool is_handled() const { return status() == MessageStatus::Handled; }
        bool is_processed() const { MessageStatus s = status(); return s == MessageStatus::Processed || s == MessageStatus::Handled; }

    private:
        struct State
        {
            MessageStatus status = MessageStatus::InProgress;
        };

        explicit MessageResult(std::shared_ptr<State> state);

        void set_status(MessageStatus status);

        std::shared_ptr<State> _state;

        friend class MessageCoordinator;
    };
}

inline tbx::MessageResult::MessageResult()
    : _state(std::make_shared<State>())
{
}

inline tbx::MessageResult::MessageResult(std::shared_ptr<State> state)
    : _state(std::move(state))
{
    if (!_state)
    {
        _state = std::make_shared<State>();
    }
}

inline tbx::MessageStatus tbx::MessageResult::status() const
{
    return _state->status;
}

inline void tbx::MessageResult::set_status(MessageStatus status)
{
    _state->status = status;
}
