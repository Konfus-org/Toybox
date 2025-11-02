#pragma once
#include "tbx/tbx_api.h"
#include <memory>
#include <string>
#include <typeinfo>
#include <utility>

namespace tbx
{
    enum class TBX_API ResultStatus
    {
        InProgress,
        Handled,
        Processed,
        Cancelled,
        Failed
    };

    struct TBX_API ResultPayloadStorage
    {
        std::shared_ptr<void> data;
        const std::type_info* type = nullptr;
    };

    // Captures lifecycle state and optional payload shared across message copies.
    // Thread-safe for concurrent reads thanks to shared_ptr-managed state.
    class TBX_API Result
    {
       public:
        Result();

        ResultStatus get_status() const;
        void set_status(ResultStatus status);
        void set_status(ResultStatus status, std::string status_message);

        const std::string& get_message() const;

        bool has_payload() const;
        void reset_payload();

        template <typename T>
        void set_payload(T value)
        {
            ResultPayloadStorage& storage = ensure_payload();
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

        operator bool() const
        {
            return get_status() != ResultStatus::Failed;
        }

       private:
        void ensure_status();
        ResultPayloadStorage& ensure_payload();
        const std::type_info* payload_type() const;
        void ensure_message() const;

        std::shared_ptr<ResultStatus> _status;
        std::shared_ptr<ResultPayloadStorage> _payload;
        std::shared_ptr<std::string> _message;
    };
}
