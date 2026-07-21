#pragma once

namespace tbx
{
    template <typename T>
    T& RuntimeRegistrations::get_data()
    {
        static_assert(
            std::is_base_of_v<RuntimeRegistrationsData, T>,
            "RuntimeRegistrations::get_data<T> requires T to derive RuntimeRegistrationsData.");

        const auto key = std::type_index(typeid(T));
        auto guard = std::lock_guard(_data_mutex);
        auto iterator = _data.find(key);
        if (iterator == _data.end())
            iterator = _data.emplace(key, std::make_unique<T>()).first;

        return static_cast<T&>(*iterator->second);
    }

    template <typename T>
    T* RuntimeRegistrations::try_get_data()
    {
        static_assert(
            std::is_base_of_v<RuntimeRegistrationsData, T>,
            "RuntimeRegistrations::try_get_data<T> requires T to derive RuntimeRegistrationsData.");

        const auto key = std::type_index(typeid(T));
        auto guard = std::lock_guard(_data_mutex);
        const auto iterator = _data.find(key);
        return iterator == _data.end() ? nullptr : static_cast<T*>(iterator->second.get());
    }
}
