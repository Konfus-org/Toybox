#pragma once
#include <memory>
#include <utility>

namespace tbx
{
    // Unique ownership (non-copyable) smart pointer.
    template <typename T, typename TDeleter = std::default_delete<T>>
    using Scope = std::unique_ptr<T, TDeleter>;

    // Constructs a Scope<T>, forwarding constructor arguments.
    template <typename T, typename... Args>
    Scope<T> make_scope(Args&&... args)
    {
        return std::make_unique<T>(std::forward<Args>(args)...);
    }

    // Shared ownership smart pointer.
    template <typename T>
    using Ref = std::shared_ptr<T>;

    // Weak non-owning reference to a shared object.
    template <typename T>
    using WeakRef = std::weak_ptr<T>;

    // Constructs a Ref<T>, forwarding constructor arguments.
    template <typename T, typename... Args>
    Ref<T> make_ref(Args&&... args)
    {
        return std::make_shared<T>(std::forward<Args>(args)...);
    }

    // Wraps a raw pointer with a Ref<T> that uses a custom deleter.
    template <typename T, typename TDeleter>
    Ref<T> make_ref(T* pointer, TDeleter&& deleter)
    {
        return Ref<T>(pointer, std::forward<TDeleter>(deleter));
    }
}
