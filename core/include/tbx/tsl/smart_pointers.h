#pragma once
#include "tbx/tsl/int.h"
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
        void operator()(TValue* value) const;
    };

    /// Unique-ownership pointer wrapper for Toybox APIs.
    /// Ownership: Owns the managed pointer and releases it on destruction unless `release` is called.
    /// Thread-safety: Not thread-safe; callers must synchronize externally when sharing instances.
    template <typename T, typename TDeleter = DefaultScopeDeleter<T>>
    class Scope
    {
      public:
        Scope();

        explicit Scope(T* ptr);

        Scope(T* ptr, TDeleter deleter);

        template <typename... TArgs>
            requires(sizeof...(TArgs) > 0 && std::is_constructible_v<T, TArgs...>)
        explicit Scope(TArgs&&... args);

        Scope(const Scope&) = delete;
        Scope& operator=(const Scope&) = delete;

        Scope(Scope&&) noexcept;
        Scope& operator=(Scope&&) noexcept;

        ~Scope();

        T* release();

        void reset(T* ptr = nullptr);

        T* get() const;

        T* get_raw() const;

        void swap(Scope& other);

        T& operator*() const;

        T* operator->() const;

        explicit operator bool() const;

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
        Ref();

        explicit Ref(T* ptr);

        template <typename TDeleter>
        Ref(T* ptr, TDeleter deleter);

        template <typename... TArgs>
            requires(sizeof...(TArgs) > 0 && std::is_constructible_v<T, TArgs...>)
        explicit Ref(TArgs&&... args);

        Ref(const Ref&);
        Ref(Ref&&) noexcept;
        Ref& operator=(const Ref&);
        Ref& operator=(Ref&&) noexcept;

        template <typename TOther>
        Ref(const Ref<TOther>& other, T* alias);

        uint64 get_use_count() const;

        void reset();

        T* get() const;

        T* get_raw() const;

        T& operator*() const;

        T* operator->() const;

        explicit operator bool() const;

      private:
        explicit Ref(std::shared_ptr<T> storage);

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
        WeakRef();
        WeakRef(const WeakRef&);
        WeakRef(WeakRef&&) noexcept;
        WeakRef& operator=(const WeakRef&);
        WeakRef& operator=(WeakRef&&) noexcept;

        WeakRef(const Ref<T>& reference);

        WeakRef& operator=(const Ref<T>& reference);

        bool is_valid() const;

        void reset();

        Ref<T> lock() const;

        T* get_raw() const;

        explicit operator bool() const;

      private:
        std::weak_ptr<T> _storage;
    };
}

#include "tbx/tsl/detail/smart_pointers.inl"
