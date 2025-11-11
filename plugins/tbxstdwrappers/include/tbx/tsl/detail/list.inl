#pragma once

namespace tbx
{
    template <typename T>
    inline List<T>::List() = default;

    template <typename T>
    inline List<T>::List(uint initial_count)
        : _storage(static_cast<size_type>(initial_count))
    {
    }

    template <typename T>
    inline List<T>::List(std::initializer_list<T> init)
        : _storage(init)
    {
    }

    template <typename T>
    inline uint List<T>::get_count() const
    {
        return static_cast<uint>(_storage.size());
    }

    template <typename T>
    inline bool List<T>::is_empty() const
    {
        return _storage.empty();
    }

    template <typename T>
    inline void List<T>::clear()
    {
        _storage.clear();
    }

    template <typename T>
    inline void List<T>::reserve(uint capacity)
    {
        _storage.reserve(static_cast<size_type>(capacity));
    }

    template <typename T>
    inline uint List<T>::get_capacity() const
    {
        return static_cast<uint>(_storage.capacity());
    }

    template <typename T>
    inline void List<T>::push_back(const T& value)
    {
        _storage.push_back(value);
    }

    template <typename T>
    inline void List<T>::push_back(T&& value)
    {
        _storage.push_back(std::move(value));
    }

    template <typename T>
    template <typename... Args>
    inline T& List<T>::emplace_back(Args&&... args)
    {
        return _storage.emplace_back(std::forward<Args>(args)...);
    }

    template <typename T>
    inline void List<T>::pop_back()
    {
        if (!_storage.empty())
        {
            _storage.pop_back();
        }
    }

    template <typename T>
    inline T* List<T>::get_raw()
    {
        return _storage.empty() ? nullptr : _storage.data();
    }

    template <typename T>
    inline const T* List<T>::get_raw() const
    {
        return _storage.empty() ? nullptr : _storage.data();
    }

    template <typename T>
    inline T& List<T>::front()
    {
        return _storage.front();
    }

    template <typename T>
    inline const T& List<T>::front() const
    {
        return _storage.front();
    }

    template <typename T>
    inline T& List<T>::back()
    {
        return _storage.back();
    }

    template <typename T>
    inline const T& List<T>::back() const
    {
        return _storage.back();
    }

    template <typename T>
    inline typename List<T>::iterator List<T>::begin()
    {
        return _storage.begin();
    }

    template <typename T>
    inline typename List<T>::iterator List<T>::end()
    {
        return _storage.end();
    }

    template <typename T>
    inline typename List<T>::const_iterator List<T>::begin() const
    {
        return _storage.begin();
    }

    template <typename T>
    inline typename List<T>::const_iterator List<T>::end() const
    {
        return _storage.end();
    }

    template <typename T>
    inline typename List<T>::const_iterator List<T>::cbegin() const
    {
        return _storage.cbegin();
    }

    template <typename T>
    inline typename List<T>::const_iterator List<T>::cend() const
    {
        return _storage.cend();
    }

    template <typename T>
    inline T& List<T>::operator[](uint index)
    {
        return _storage[static_cast<size_type>(index)];
    }

    template <typename T>
    inline const T& List<T>::operator[](uint index) const
    {
        return _storage[static_cast<size_type>(index)];
    }

    template <typename T>
    inline T& List<T>::at(uint index)
    {
        return _storage.at(static_cast<size_type>(index));
    }

    template <typename T>
    inline const T& List<T>::at(uint index) const
    {
        return _storage.at(static_cast<size_type>(index));
    }

    template <typename T>
    inline typename List<T>::Storage& List<T>::std_vector()
    {
        return _storage;
    }

    template <typename T>
    inline const typename List<T>::Storage& List<T>::std_vector() const
    {
        return _storage;
    }

    template <typename T>
    inline void List<T>::swap(List& other)
    {
        _storage.swap(other._storage);
    }
}
