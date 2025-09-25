#pragma once
#include "Tbx/DllExport.h"
#include <unordered_map>
#include <typeindex>
#include <any>

namespace Tbx
{
    struct TBX_EXPORT BlockCollection
    {
        /// <summary>
        /// Constructs data of type <typeparamref name="T"/> and stores it within the toy.
        /// </summary>
        template <typename T, typename... Args>
        T& Add(Args&&... args)
        {
            const auto typeIndex = std::type_index(typeid(std::remove_cvref_t<T>));
            auto& block = _blocks[typeIndex];
            block = T(std::forward<Args>(args)...);
            return std::any_cast<T&>(block);
        }

        /// <summary>
        /// Removes stored data of type <typeparamref name="T"/>.
        /// </summary>
        template <typename T>
        void Remove()
        {
            const auto typeIndex = std::type_index(typeid(std::remove_cvref_t<T>));
            _blocks.erase(typeIndex);
        }

        /// <summary>
        /// Retrieves stored data of type <typeparamref name="T"/>.
        /// </summary>
        template <typename T>
        T& Get()
        {
            const auto typeIndex = std::type_index(typeid(std::remove_cvref_t<T>));
            auto& block = _blocks.at(typeIndex);
            return std::any_cast<T&>(block);
        }

        /// <summary>
        /// Retrieves stored data of type <typeparamref name="T"/>.
        /// </summary>
        template <typename T>
        const T& Get() const
        {
            const auto typeIndex = std::type_index(typeid(std::remove_cvref_t<T>));
            const auto& block = _blocks.at(typeIndex);
            return std::any_cast<const T&>(block);
        }

        /// <summary>
        /// Determines whether data of type <typeparamref name="T"/> is stored on this toy.
        /// </summary>
        template <typename T>
        bool Contains() const
        {
            const auto typeIndex = std::type_index(typeid(std::remove_cvref_t<T>));
            return _blocks.contains(typeIndex);
        }

    private:
        std::unordered_map<std::type_index, std::any> _blocks = {};
    };
}