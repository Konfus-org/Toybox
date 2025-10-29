#pragma once
#include <memory>
#include <typeinfo>
#include <utility>

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

    struct MessageResultValueStorage
    {
        std::shared_ptr<void> data;
        const std::type_info* type = nullptr;
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
        bool is_processed() const
        {
            MessageStatus s = status();
            return s == MessageStatus::Processed || s == MessageStatus::Handled;
        }

        bool has_value() const;
        void reset_value();

        template <typename T>
        void set_value(T value)
        {
            MessageResultValueStorage& storage = ensure_storage();
            storage.data = std::make_shared<T>(std::move(value));
            storage.type = &typeid(T);
        }

        template <typename T>
        T* try_get()
        {
            if (!_storage || !_storage->data)
                return nullptr;
            if (value_type() != &typeid(T))
                return nullptr;
            return std::static_pointer_cast<T>(_storage->data).get();
        }

        template <typename T>
        const T* try_get() const
        {
            if (!_storage || !_storage->data)
                return nullptr;
            if (value_type() != &typeid(T))
                return nullptr;
            return std::static_pointer_cast<T>(_storage->data).get();
        }

        template <typename T>
        T value_or(T fallback) const
        {
            const T* value = try_get<T>();
            if (value)
            {
                return *value;
            }
            return fallback;
        }

    private:
        void set_status(MessageStatus status);
        void ensure_status();
        MessageResultValueStorage& ensure_storage();
        const std::type_info* value_type() const;

        std::shared_ptr<MessageStatus> _status;
        std::shared_ptr<MessageResultValueStorage> _storage;

        friend class MessageCoordinator;
    };
}
