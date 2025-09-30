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
    /// Use Ref when sharing ownership of an object across multiple systems.
    /// </summary>
    template <typename T, typename... Args>
    Ref<T> MakeRef(Args&&... args) { return std::make_shared<T>(std::forward<Args>(args)...); }

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

    /// <summary>
    /// Use ExclusiveRef when a single owner must control the object's lifetime.
    /// </summary>
    template <typename T, typename... Args>
    ExclusiveRef<T> MakeExclusive(Args&&... args) { return std::make_unique<T>(std::forward<Args>(args)...); }

    template <typename T>
    struct IsRef : std::false_type {};

    template <typename U>
    struct IsRef<Ref<U>> : std::true_type {};

    template <typename U>
    struct IsRef<WeakRef<U>> : std::true_type {};

    template <typename U>
    struct IsRef<ExclusiveRef<U>> : std::true_type {};
}
