#pragma once

#include <memory>
#include <type_traits>

namespace Tbx
{
    /// <summary>
    /// Use Ref when sharing ownership of an object across multiple systems.
    /// </summary>
    template <typename T>
    using Ref = std::shared_ptr<T>;

    /// <summary>
    /// Use WeakRef when observing an object without affecting its lifetime.
    /// </summary>
    template <typename T>
    using WeakRef = std::weak_ptr<T>;

    /// <summary>
    /// Use ExclusiveRef when a single owner must control the object's lifetime.
    /// </summary>
    template <typename T>
    using ExclusiveRef = std::unique_ptr<T>;
}

template <typename T>
struct is_smart_ptr : std::false_type {};

template <typename U>
struct is_smart_ptr<Ref<U>> : std::true_type {};

template <typename U>
struct is_smart_ptr<WeakRef<U>> : std::true_type {};

template <typename U>
struct is_smart_ptr<ExclusiveRef<U>> : std::true_type {};
