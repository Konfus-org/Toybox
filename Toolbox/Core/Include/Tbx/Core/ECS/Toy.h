#pragma once
#include "Tbx/Core/DllExport.h"
#include "Tbx/Core/ECS/Block.h"
#include "Tbx/Core/ECS/IBoxable.h"
#include <unordered_map>
#include <vector>
#include <typeindex>
#include <memory>

namespace Tbx
{
    /// <summary>
    /// A toy is really just a collection of blocks or "components". 
    /// It is a way to group blocks together to make a "game object" that has some behaviors.
    /// </summary>
    struct EXPORT Toy : public IBoxable
    {
    public:
        /// <summary>
        /// Gets a block of the specified type.
        /// </summary>
        template <typename T>
        std::shared_ptr<T> GetBlock() const
        {
            if (auto it = _blocks.find(std::type_index(typeid(T))); it != _blocks.end())
            {
                return std::dynamic_pointer_cast<T>(it->second);
            }
            return nullptr;
        }

        /// <summary>
        /// Adds a block to the toy.
        /// </summary>
        template <typename T>
        void AddBlock(std::shared_ptr<Block<T>> block)
        {
            _blocks[std::type_index(typeid(T))] = block;
        }

    private:
        /// <summary>
        /// The blocks that make up the toy.
        /// </summary>
        std::unordered_map<std::type_index, std::shared_ptr<IBlock>> _blocks;
    };
}