#pragma once

#include <memory>
#include <stdexcept>
#include <string>
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

    struct MessageResultPayloadStorage
    {
        std::shared_ptr<void> data;
        const std::type_info* type = nullptr;
    };

    class MessageCoordinator;

    /// \brief Captures the lifecycle and optional payload produced by a dispatched message.
    class MessageResult
    {
    public:
        MessageResult();

        MessageStatus status() const;
        void set_status(MessageStatus status);
        void set_status(MessageStatus status, std::string reason);
        void set_failure(std::string reason) { set_status(MessageStatus::Failed, std::move(reason)); }

        bool is_in_progress() const { return status() == MessageStatus::InProgress; }
        bool is_cancelled() const { return status() == MessageStatus::Cancelled; }
        bool is_failed() const { return status() == MessageStatus::Failed; }
        bool is_handled() const { return status() == MessageStatus::Handled; }
        bool is_processed() const
        {
            MessageStatus s = status();
            return s == MessageStatus::Processed || s == MessageStatus::Handled;
        }
        bool succeeded() const { return is_processed(); }
        explicit operator bool() const { return succeeded(); }

        const std::string& why() const;
        void clear_reason();

        bool has_payload() const;
        void reset_payload();

        template <typename T>
        void set_payload(T value)
        {
            MessageResultPayloadStorage& storage = ensure_payload();
            storage.data = std::make_shared<T>(std::move(value));
            storage.type = &typeid(T);
        }

        template <typename T>
        bool has_payload() const
        {
            return this->has_payload() && payload_type() == &typeid(T);
        }

        template <typename T>
        T* try_get_payload()
        {
            if (!has_payload<T>())
            {
                return nullptr;
            }
            return std::static_pointer_cast<T>(_payload->data).get();
        }

        template <typename T>
        const T* try_get_payload() const
        {
            if (!has_payload<T>())
            {
                return nullptr;
            }
            return std::static_pointer_cast<T>(_payload->data).get();
        }

        template <typename T>
        T& get_payload()
        {
            T* payload = try_get_payload<T>();
            if (!payload)
            {
                throw std::bad_cast();
            }
            return *payload;
        }

        template <typename T>
        const T& get_payload() const
        {
            const T* payload = try_get_payload<T>();
            if (!payload)
            {
                throw std::bad_cast();
            }
            return *payload;
        }

        template <typename T>
        T payload_or(T fallback) const
        {
            const T* value = try_get_payload<T>();
            if (value)
            {
                return *value;
            }
            return fallback;
        }

    private:
        void ensure_status();
        MessageResultPayloadStorage& ensure_payload();
        const std::type_info* payload_type() const;
        void ensure_reason() const;

        std::shared_ptr<MessageStatus> _status;
        std::shared_ptr<MessageResultPayloadStorage> _payload;
        std::shared_ptr<std::string> _reason;

        friend class MessageCoordinator;
    };
}

