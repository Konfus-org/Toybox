#pragma once
#include "Tbx/DllExport.h"
#include "Tbx/Debug/Tracers.h"
#include "Tbx/Math/Int.h"
#include "Tbx/Memory/MemoryPool.h"
#include "Tbx/Memory/Refs.h"
#include <any>
#include <memory>
#include <typeindex>
#include <type_traits>
#include <unordered_map>
#include <utility>

namespace Tbx
{
    struct BlockPoolRegistry
    {
        static constexpr uint64 DefaultBlockCapacity = 200;

        template <typename TBlock>
        static MemoryPool<TBlock>& GetPool()
        {
            auto& pools = Pools();
            const auto typeIndex = std::type_index(typeid(TBlock));
            auto found = pools.find(typeIndex);
            if (found == pools.end())
            {
                auto inserted = pools.emplace(typeIndex, MemoryPool<TBlock>(DefaultBlockCapacity));
                TBX_TRACE_INFO("BlockCollection: allocated pool for %s with capacity %llu", typeid(TBlock).name(), DefaultBlockCapacity);
                return std::any_cast<MemoryPool<TBlock>&>(inserted.first->second);
            }

            return std::any_cast<MemoryPool<TBlock>&>(found->second);
        }

        template <typename TBlock>
        static void Reserve(uint64 additionalCapacity)
        {
            auto& pool = GetPool<TBlock>();
            pool.Reserve(additionalCapacity);
            TBX_TRACE_INFO("BlockCollection: resized pool for %s by %llu (new capacity=%llu)", typeid(TBlock).name(), additionalCapacity, pool.Capacity());
        }

    private:
        static std::unordered_map<std::type_index, std::any>& Pools()
        {
            static std::unordered_map<std::type_index, std::any> pools;
            return pools;
        }
    };

    struct TBX_EXPORT BlockCollection
    {
    private:
        template <typename TBlock>
        static MemoryPool<TBlock>& AcquirePool()
        {
            return BlockPoolRegistry::GetPool<TBlock>();
        }

    public:
        /// <summary>
        /// Constructs data of type <typeparamref name="T"/> and stores it within the toy.
        /// </summary>
        template <typename T, typename... Args>
        Ref<T> Add(Args&&... args)
        {
            using BlockType = std::remove_cvref_t<T>;
            const auto typeIndex = std::type_index(typeid(BlockType));

            auto& pool = AcquirePool<BlockType>();
            if (pool.IsFull())
            {
                TBX_TRACE_WARNING("BlockCollection: pool for %s is exhausted. Call BlockCollection::Reserve<%s>(additionalCapacity) before adding more instances.", typeid(BlockType).name(), typeid(BlockType).name());
                return Ref<BlockType>();
            }

            auto block = pool.Provide(std::forward<Args>(args)...);
            if (block == nullptr)
            {
                return block;
            }

            _blocks[typeIndex] = Ref<void>(block);
            return block;
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
        Ref<T> Get()
        {
            using BlockType = std::remove_cvref_t<T>;
            const auto typeIndex = std::type_index(typeid(BlockType));
            auto& block = _blocks.at(typeIndex);
            auto typed = std::static_pointer_cast<BlockType>(block);
            return typed;
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

        /// <summary>
        /// Ensures additional capacity is available for the specified block type.
        /// </summary>
        template <typename T>
        void Reserve(uint64 additionalCapacity)
        {
            using BlockType = std::remove_cvref_t<T>;
            BlockPoolRegistry::Reserve<BlockType>(additionalCapacity);
        }

        /// <summary>
        /// Attempts to retrieve a shared pointer to the stored block of type <typeparamref name="T"/>.
        /// </summary>
        template <typename T>
        bool TryGet(Ref<T>& block)
        {
            using BlockType = std::remove_cvref_t<T>;
            const auto typeIndex = std::type_index(typeid(BlockType));
            auto iter = _blocks.find(typeIndex);
            if (iter == _blocks.end())
            {
                block = nullptr;
                return false;
            }

            block = std::static_pointer_cast<BlockType>(iter->second);
            return block != nullptr;
        }

    private:
        std::unordered_map<std::type_index, Ref<void>> _blocks = {};
    };
}

