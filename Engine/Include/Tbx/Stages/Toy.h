#pragma once
#include "Tbx/DllExport.h"
#include "Tbx/Collections/Collection.h"
#include "Tbx/Ids/Uid.h"
#include "Tbx/Math/Int.h"
#include "Tbx/Memory/MemoryPool.h"
#include "Tbx/Memory/Refs.h"
#include "Tbx/Stages/Blocks.h"
#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>

namespace Tbx
{
    class Toy;

    template <typename T>
    inline constexpr bool IsToy = std::is_base_of_v<Toy, typename std::remove_cv<typename std::remove_reference<T>::type>::type>;

    /// <summary>
    /// Identifies a toy via a unique ID and name.
    /// </summary>
    struct TBX_EXPORT ToyHandle
    {
        std::string Name = "";
        Uid Id = Uid::Generate();
    };

    /// <summary>
    /// Represents a toy in a hierarchy with arbitrary typed blocks.
    /// </summary>
    class TBX_EXPORT Toy
    {
    public:
        static constexpr uint64 DefaultPoolCapacity = 1024;

        /// <summary>
        /// Creates a toy using the internal memory pool to ensure contiguous storage.
        /// </summary>
        template <typename TToy = Toy, typename... Args>
        static Ref<TToy> Make(Args&&... args)
        {
            static_assert(std::is_base_of_v<Toy, TToy>, "Toy::Make can only construct Toy types");
            auto& pool = AcquirePool<TToy>();
            return pool.Provide(std::forward<Args>(args)...);
        }

        /// <summary>
        /// Updates this toy and recursively updates its children if enabled.
        /// </summary>
        void Update();

        /// <summary>
        /// Adds a block of type <typeparamref name="T"/> or constructs a child toy and returns its shared reference.
        /// </summary>
        template <typename T, typename... Args>
        Ref<T> Add(Args&&... args)
        {
            if constexpr (IsToy<T>)
            {
                auto child = Toy::Make<T>(std::forward<Args>(args)...);
                Children.Add(child);
                return child;
            }
            else
            {
                return Blocks.Add<T>(std::forward<Args>(args)...);
            }
        }

        /// <summary>
        /// Determines whether this toy contains a block of type <typeparamref name="T"/> or a child toy matching the request.
        /// </summary>
        template <typename T>
        bool Has() const
        {
            if constexpr (IsToy<T>)
            {
                return Children.Any([](const Ref<Toy>& child)
                {
                    return std::dynamic_pointer_cast<T>(child) != nullptr;
                });
            }
            else
            {
                return Blocks.Contains<T>();
            }
        }

        /// <summary>
        /// Removes the block of type <typeparamref name="T"/> or child toy from this toy if it exists.
        /// </summary>
        template <typename T>
        void Remove()
        {
            using RequestedType = typename std::remove_cv<typename std::remove_reference<T>::type>::type;
            if constexpr (IsToy<T>)
            {
                Children.Remove([](const Ref<Toy>& child)
                {
                    return std::dynamic_pointer_cast<RequestedType>(child) != nullptr;
                });
            }
            else
            {
                Blocks.Remove<RequestedType>();
            }
        }

        /// <summary>
        /// Retrieves the shared reference to the block of type <typeparamref name="T"/> or the matching child toy.
        /// </summary>
        template <typename T>
        Ref<T> Get() const
        {
            if constexpr (IsToy<T>)
            {
                Ref<T> child;
                if (!TryGet<T>(child))
                {
                    throw std::out_of_range("Toy::Get: child not found");
                }

                return child;
            }
            else
            {
                return Blocks.Get<T>();
            }
        }

        /// <summary>
        /// Attempts to retrieve a shared reference to the block of type <typeparamref name="T"/> or the matching child toy if present.
        /// </summary>
        template <typename T>
        bool TryGet(Ref<T>& item) const
        {
            if constexpr (IsToy<T>)
            {
                for (const auto& child : Children)
                {
                    if (auto casted = std::dynamic_pointer_cast<T>(child))
                    {
                        item = casted;
                        return true;
                    }
                }

                item = nullptr;
                return false;
            }
            else
            {
                return Blocks.TryGet<T>(item);
            }
        }

        /// <summary>
        /// Retrieves the shared reference to a child toy with the provided identifier.
        /// </summary>
        Ref<Toy> Get(const Uid& id) const
        {
            Ref<Toy> child;
            if (!TryGet(id, child))
            {
                throw std::out_of_range("Toy::Get: child not found");
            }

            return child;
        }

        /// <summary>
        /// Attempts to retrieve the child toy matching the provided identifier.
        /// </summary>
        bool TryGet(const Uid& id, Ref<Toy>& child) const
        {
            for (const auto& candidate : Children)
            {
                if (candidate && candidate->Handle.Id == id)
                {
                    child = candidate;
                    return true;
                }
            }

            child = nullptr;
            return false;
        }

    protected:
        /// <summary>
        /// Creates a toy.
        /// </summary>
        Toy() = default;

        /// <summary>
        /// Creates a toy with the given name.
        /// </summary>
        Toy(const std::string& name);

        /// <summary>
        /// Hook called during <see cref="Update"/> before children are updated.
        /// </summary>
        virtual void OnUpdate()
        {
        }

    private:
        template <typename TToy>
        static MemoryPool<TToy>& AcquirePool()
        {
            static auto pool = MemoryPool<TToy>(DefaultPoolCapacity);
            return pool;
        }

        // To allow the pool to create new toys, we need to friend it
        template <typename TObject>
        friend struct MemoryPool;

    public:
        ToyHandle Handle = {};
        BlockCollection Blocks = {};
        Collection<Ref<Toy>> Children = {};
        bool Enabled = true;
    };
}

