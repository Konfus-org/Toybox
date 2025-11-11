#pragma once

namespace tbx
{
    template <typename TValue>
    inline void DefaultScopeDeleter<TValue>::operator()(TValue* value) const
    {
        delete value;
    }

    template <typename T, typename TDeleter>
    inline Scope<T, TDeleter>::Scope() = default;

    template <typename T, typename TDeleter>
    inline Scope<T, TDeleter>::Scope(T* ptr)
        : _storage(ptr)
    {
    }

    template <typename T, typename TDeleter>
    inline Scope<T, TDeleter>::Scope(T* ptr, TDeleter deleter)
        : _storage(ptr, std::move(deleter))
    {
    }

    template <typename T, typename TDeleter>
    template <typename... TArgs>
        requires(sizeof...(TArgs) > 0 && std::is_constructible_v<T, TArgs...>)
    inline Scope<T, TDeleter>::Scope(TArgs&&... args)
        : _storage(new T(std::forward<TArgs>(args)...))
    {
    }

    template <typename T, typename TDeleter>
    inline Scope<T, TDeleter>::Scope(Scope&&) noexcept = default;

    template <typename T, typename TDeleter>
    inline Scope<T, TDeleter>& Scope<T, TDeleter>::operator=(Scope&&) noexcept = default;

    template <typename T, typename TDeleter>
    inline Scope<T, TDeleter>::~Scope() = default;

    template <typename T, typename TDeleter>
    inline T* Scope<T, TDeleter>::release()
    {
        return _storage.release();
    }

    template <typename T, typename TDeleter>
    inline void Scope<T, TDeleter>::reset(T* ptr)
    {
        _storage.reset(ptr);
    }

    template <typename T, typename TDeleter>
    inline T* Scope<T, TDeleter>::get() const
    {
        return _storage.get();
    }

    template <typename T, typename TDeleter>
    inline T* Scope<T, TDeleter>::get_raw() const
    {
        return _storage.get();
    }

    template <typename T, typename TDeleter>
    inline void Scope<T, TDeleter>::swap(Scope& other)
    {
        _storage.swap(other._storage);
    }

    template <typename T, typename TDeleter>
    inline T& Scope<T, TDeleter>::operator*() const
    {
        return *_storage;
    }

    template <typename T, typename TDeleter>
    inline T* Scope<T, TDeleter>::operator->() const
    {
        return _storage.get();
    }

    template <typename T, typename TDeleter>
    inline Scope<T, TDeleter>::operator bool() const
    {
        return static_cast<bool>(_storage);
    }

    template <typename T>
    inline Ref<T>::Ref() = default;

    template <typename T>
    inline Ref<T>::Ref(T* ptr)
        : _storage(ptr)
    {
    }

    template <typename T>
    template <typename TDeleter>
    inline Ref<T>::Ref(T* ptr, TDeleter deleter)
        : _storage(ptr, std::move(deleter))
    {
    }

    template <typename T>
    template <typename... TArgs>
        requires(sizeof...(TArgs) > 0 && std::is_constructible_v<T, TArgs...>)
    inline Ref<T>::Ref(TArgs&&... args)
        : _storage(std::make_shared<T>(std::forward<TArgs>(args)...))
    {
    }

    template <typename T>
    inline Ref<T>::Ref(const Ref&) = default;

    template <typename T>
    inline Ref<T>::Ref(Ref&&) noexcept = default;

    template <typename T>
    inline Ref<T>& Ref<T>::operator=(const Ref&) = default;

    template <typename T>
    inline Ref<T>& Ref<T>::operator=(Ref&&) noexcept = default;

    template <typename T>
    template <typename TOther>
    inline Ref<T>::Ref(const Ref<TOther>& other, T* alias)
        : _storage(other._storage, alias)
    {
    }

    template <typename T>
    inline uint64 Ref<T>::get_use_count() const
    {
        return static_cast<uint64>(_storage.use_count());
    }

    template <typename T>
    inline void Ref<T>::reset()
    {
        _storage.reset();
    }

    template <typename T>
    inline T* Ref<T>::get() const
    {
        return _storage.get();
    }

    template <typename T>
    inline T* Ref<T>::get_raw() const
    {
        return _storage.get();
    }

    template <typename T>
    inline T& Ref<T>::operator*() const
    {
        return *_storage;
    }

    template <typename T>
    inline T* Ref<T>::operator->() const
    {
        return _storage.get();
    }

    template <typename T>
    inline Ref<T>::operator bool() const
    {
        return static_cast<bool>(_storage);
    }

    template <typename T>
    inline Ref<T>::Ref(std::shared_ptr<T> storage)
        : _storage(std::move(storage))
    {
    }

    template <typename T>
    inline WeakRef<T>::WeakRef() = default;

    template <typename T>
    inline WeakRef<T>::WeakRef(const WeakRef&) = default;

    template <typename T>
    inline WeakRef<T>::WeakRef(WeakRef&&) noexcept = default;

    template <typename T>
    inline WeakRef<T>& WeakRef<T>::operator=(const WeakRef&) = default;

    template <typename T>
    inline WeakRef<T>& WeakRef<T>::operator=(WeakRef&&) noexcept = default;

    template <typename T>
    inline WeakRef<T>::WeakRef(const Ref<T>& reference)
        : _storage(reference._storage)
    {
    }

    template <typename T>
    inline WeakRef<T>& WeakRef<T>::operator=(const Ref<T>& reference)
    {
        _storage = reference._storage;
        return *this;
    }

    template <typename T>
    inline bool WeakRef<T>::is_valid() const
    {
        return !_storage.expired();
    }

    template <typename T>
    inline void WeakRef<T>::reset()
    {
        _storage.reset();
    }

    template <typename T>
    inline Ref<T> WeakRef<T>::lock() const
    {
        return Ref<T>(_storage.lock());
    }

    template <typename T>
    inline T* WeakRef<T>::get_raw() const
    {
        std::shared_ptr<T> shared = _storage.lock();
        return shared ? shared.get() : nullptr;
    }

    template <typename T>
    inline WeakRef<T>::operator bool() const
    {
        return is_valid();
    }
}
