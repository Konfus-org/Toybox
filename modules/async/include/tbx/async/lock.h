#pragma once
#include "tbx/tbx_api.h"
#include <memory>
#include <mutex>
#include <utility>

namespace tbx
{
    template <typename TValue>
    class Lock;

    template <typename TValue>
    class ThreadSafe
    {
      public:
        ThreadSafe() = default;
        template <typename... TArgs>
        explicit ThreadSafe(TArgs&&... args)
            : _value(std::forward<TArgs>(args)...)
            , _mutex()
        {
        }

        ThreadSafe(const ThreadSafe&) = default;
        ThreadSafe& operator=(const ThreadSafe&) = default;
        ThreadSafe(ThreadSafe&&) noexcept = default;
        ThreadSafe& operator=(ThreadSafe&&) noexcept = default;

        Lock<TValue> lock() const
        {
            return Lock<TValue>(this);
        }

      private:
        mutable std::mutex _mutex;
        TValue _value;

        friend class Lock<TValue>;
    };

    template <typename TValue>
    class Lock final
    {
      public:
        Lock(const ThreadSafe<TValue>* owner)
            : _owner(owner)
        {
            if (_owner)
            {
                _owner->_mutex.lock();
            }
        }
        ~Lock()
        {
            release();
        }

        Lock(const Lock&) = delete;
        Lock& operator=(const Lock&) = delete;
        Lock(Lock&&) = delete;
        Lock& operator=(Lock&&) noexcept = delete;

        TValue& get() const
        {
            return const_cast<TValue&>(_owner->_value);
        }

        operator TValue&() const
        {
            return get();
        }

        TValue* operator->() const
        {
            return &_owner->_value;
        }

      private:
        void release()
        {
            if (_owner)
            {
                _owner->_mutex.unlock();
                _owner = nullptr;
            }
        }

        const ThreadSafe<TValue>* _owner;
    };
}
