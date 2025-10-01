#pragma once
#include <functional>
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

    template <typename T>
    struct RefHasher
    {
        size_t operator()(const Ref<T>& ref) const noexcept
        {
            return std::hash<const T*>()(ref.get());
        }
    };

    template <typename T>
    struct RefEqual
    {
        bool operator()(const Ref<T>& lhs, const Ref<T>& rhs) const noexcept
        {
            return lhs.get() == rhs.get();
        }
    };

    template <typename T>
    struct ExclusiveRefHasher
    {
        size_t operator()(const ExclusiveRef<T>& ref) const noexcept
        {
            return std::hash<const T*>()(ref.get());
        }
    };

    template <typename T>
    struct ExclusiveRefEqual
    {
        bool operator()(const ExclusiveRef<T>& lhs, const ExclusiveRef<T>& rhs) const noexcept
        {
            return lhs.get() == rhs.get();
        }
    };

    template <typename T>
    struct WeakRefHasher
    {
        size_t operator()(const WeakRef<T>& ref) const noexcept
        {
            auto locked = ref.lock();
            return std::hash<const T*>()(locked.get());
        }
    };

    template <typename T>
    struct WeakRefEqual
    {
        bool operator()(const WeakRef<T>& lhs, const WeakRef<T>& rhs) const noexcept
        {
            auto lhsLocked = lhs.lock();
            auto rhsLocked = rhs.lock();
            return lhsLocked.get() == rhsLocked.get();
        }
    };
}
