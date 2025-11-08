#pragma once
#include "tbx/tsl/int.h"
#include <memory>
#include <utility>

namespace tbx
{
    /// Minimal unique-ownership pointer.
    /// This means the pointer has a single owner, and the storage is freed when the owner is
    /// destroyed.
    template <typename T, typename TDeleter = std::default_delete<T>>
    class Scope
    {
      public:
        Scope() = default;
        Scope(T* ptr)
            : _storage(ptr)
        {
        }
        Scope(T* ptr, TDeleter deleter)
            : _storage(ptr, std::move(deleter))
        {
        }

        Scope(const Scope&) = delete;
        Scope& operator=(const Scope&) = delete;

        Scope(Scope&& other) noexcept = default;
        Scope& operator=(Scope&& other) noexcept = default;

        ~Scope() = default;

        /// Releases ownership without deleting the pointer.
        T* release()
        {
            return _storage.release();
        }

        /// Deletes the currently held pointer and adopts a new one.
        void reset(T* ptr = nullptr)
        {
            _storage.reset(ptr);
        }

        /// Returns the managed pointer.
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

        operator bool() const
        {
            return static_cast<bool>(_storage);
        }

      private:
        std::unique_ptr<T, TDeleter> _storage;
    };

    /// Shared ownership pointer.
    /// This means the pointer can have multiple owners, and the storage is freed when the last
    /// owner is destroyed.
    template <typename T>
    class Ref
    {
      public:
        Ref() = default;
        Ref(T* ptr)
            : _storage(ptr)
        {
        }
        Ref(const T& value)
            : _storage(std::make_shared<T>(value))
        {
        }
        template <typename TDeleter>
        Ref(T* ptr, TDeleter deleter)
            : _storage(ptr, std::move(deleter))
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

        operator bool()
        {
            return static_cast<bool>(_storage);
        }

      private:
        std::shared_ptr<T> _storage;
    };

    /// Non-owning observer for Ref-managed storage.
    /// This means the pointer does not own the storage, and the storage may be freed while this is
    /// alive.
    template <typename T>
    class WeakRef
    {
      public:
        WeakRef() = default;
        WeakRef(const Ref<T>& reference)
            : _storage(reference.std_ptr())
        {
        }

        WeakRef& operator=(const Ref<T>& reference)
        {
            _storage = reference.std_ptr();
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
            return _storage.lock()->get_raw();
        }

        operator bool() const
        {
            return is_valid();
        }

      private:
        std::weak_ptr<T> _storage;
    };
}
