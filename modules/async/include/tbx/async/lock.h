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

        ThreadSafe(const ThreadSafe& other)
            : _value()
            , _mutex()
        {
            std::lock_guard<std::mutex> guard(other._mutex);
            _value = other._value;
        }

        ThreadSafe& operator=(const ThreadSafe& other)
        {
            if (this != &other)
            {
                std::scoped_lock<std::mutex, std::mutex> guard(_mutex, other._mutex);
                _value = other._value;
            }
            return *this;
        }

        ThreadSafe(ThreadSafe&& other)
            : _value()
            , _mutex()
        {
            std::lock_guard<std::mutex> guard(other._mutex);
            _value = std::move(other._value);
        }

        ThreadSafe& operator=(ThreadSafe&& other)
        {
            if (this != &other)
            {
                std::scoped_lock<std::mutex, std::mutex> guard(_mutex, other._mutex);
                _value = std::move(other._value);
            }
            return *this;
        }

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
