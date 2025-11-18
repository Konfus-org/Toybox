#pragma once
#include "tbx/std/int.h"
#include <memory>
#include <type_traits>
#include <utility>

namespace tbx
{
    /// Provides the default deletion strategy for `Scope` when no custom deleter is supplied.
    /// Ownership: Deletes the owned pointer using `delete` when invoked.
    /// Thread-safety: Stateless and thread-safe.
    template <typename TValue>
    struct DefaultScopeDeleter
    {
        void operator()(TValue* value) const
        {
            delete value;
        }
    };

    /// Unique-ownership pointer wrapper for Toybox APIs.
    /// Ownership: Owns the managed pointer and releases it on destruction unless `release` is called.
    /// Thread-safety: Not thread-safe; callers must synchronize externally when sharing instances.
    template <typename T, typename TDeleter = DefaultScopeDeleter<T>>
    class Scope
    {
      public:
        Scope() = default;

        explicit Scope(T* ptr)
            : _storage(ptr)
        {
        }

        Scope(T* ptr, TDeleter deleter)
            : _storage(ptr, std::move(deleter))
        {
        }

        template <typename... TArgs>
            requires(sizeof...(TArgs) > 0 && std::is_constructible_v<T, TArgs...>)
        explicit Scope(TArgs&&... args)
            : _storage(new T(std::forward<TArgs>(args)...))
        {
        }

        Scope(const Scope&) = delete;
        Scope& operator=(const Scope&) = delete;

        Scope(Scope&&) noexcept = default;
        Scope& operator=(Scope&&) noexcept = default;

        ~Scope() = default;

        T* release()
        {
            return _storage.release();
        }

        void reset(T* ptr = nullptr)
        {
            _storage.reset(ptr);
        }

        T* get() const
        {
            return _storage.get();
        }

        T* get_raw() const
        {
            return _storage.get();
        }

        void swap(Scope& other)
        {
            _storage.swap(other._storage);
        }

        T& operator*() const
        {
            return *_storage;
        }

        T* operator->() const
        {
            return _storage.get();
        }

        explicit operator bool() const
        {
            return static_cast<bool>(_storage);
        }

      private:
        std::unique_ptr<T, TDeleter> _storage;
    };

    /// Shared-ownership pointer wrapper for Toybox APIs.
    /// Ownership: Shares ownership across copies; storage is destroyed when the last `Ref` releases it.
    /// Thread-safety: Reference counting is thread-safe, but access to the underlying object is not synchronized.
    template <typename T>
    class Ref
    {
      public:
        Ref() = default;

        explicit Ref(T* ptr)
            : _storage(ptr)
        {
        }

        template <typename TDeleter>
        Ref(T* ptr, TDeleter deleter)
            : _storage(ptr, std::move(deleter))
        {
        }

        template <typename... TArgs>
            requires(sizeof...(TArgs) > 0 && std::is_constructible_v<T, TArgs...>)
        explicit Ref(TArgs&&... args)
            : _storage(std::make_shared<T>(std::forward<TArgs>(args)...))
        {
        }

        Ref(const Ref&) = default;
        Ref(Ref&&) noexcept = default;
        Ref& operator=(const Ref&) = default;
        Ref& operator=(Ref&&) noexcept = default;

        template <typename TOther>
        Ref(const Ref<TOther>& other, T* alias)
            : _storage(other._storage, alias)
        {
        }

        uint64 get_use_count() const
        {
            return static_cast<uint64>(_storage.use_count());
        }

        void reset()
        {
            _storage.reset();
        }

        T* get() const
        {
            return _storage.get();
        }

        T* get_raw() const
        {
            return _storage.get();
        }

        T& operator*() const
        {
            return *_storage;
        }

        T* operator->() const
        {
            return _storage.get();
        }

        explicit operator bool() const
        {
            return static_cast<bool>(_storage);
        }

      private:
        explicit Ref(std::shared_ptr<T> storage)
            : _storage(std::move(storage))
        {
        }

        std::shared_ptr<T> _storage;

        template <typename>
        friend class Ref;

        template <typename>
        friend class WeakRef;
    };

    /// Non-owning view of storage managed by `Ref` instances.
    /// Ownership: Does not own the pointed-to object; callers must call `lock` to obtain a `Ref` before use.
    /// Thread-safety: Thread-safe for observing lifetime; returned `Ref` shares the same considerations as `Ref` itself.
    template <typename T>
    class WeakRef
    {
      public:
        WeakRef() = default;
        WeakRef(const WeakRef&) = default;
        WeakRef(WeakRef&&) noexcept = default;
        WeakRef& operator=(const WeakRef&) = default;
        WeakRef& operator=(WeakRef&&) noexcept = default;

        WeakRef(const Ref<T>& reference)
            : _storage(reference._storage)
        {
        }

        WeakRef& operator=(const Ref<T>& reference)
        {
            _storage = reference._storage;
            return *this;
        }

        bool is_valid() const
        {
            return !_storage.expired();
        }

        void reset()
        {
            _storage.reset();
        }

        Ref<T> lock() const
        {
            return Ref<T>(_storage.lock());
        }

        T* get_raw() const
        {
            std::shared_ptr<T> shared = _storage.lock();
            return shared ? shared.get() : nullptr;
        }

        explicit operator bool() const
        {
            return is_valid();
        }

      private:
        std::weak_ptr<T> _storage;
    };
}
