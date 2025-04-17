#pragma once
#include "Tbx/Core/DllExport.h"
#include "Tbx/Core/TBS/Block.h"
#include "Tbx/Core/TBS/IBoxable.h"
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
    struct Toy : public IBoxable
    {
    public:
        /// <summary>
        /// Gets a block of the specified type.
        /// </summary>
        template <typename T>
        EXPORT std::shared_ptr<T> GetBlock() const
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
        EXPORT void AddBlock(std::shared_ptr<Block<T>> block)
        {
            _blocks[std::type_index(typeid(T))] = block;
        }

        /// <summary>
        /// Creates and adds a block to the toy then returns it.
        /// </summary>
        template <typename T>
        EXPORT std::shared_ptr<T> AddBlock()
        {
            std::shared_ptr<Block<T>> block = std::make_shared<Block<T>>();
            _blocks[std::type_index(typeid(T))] = block;
            return block->Get();
        }


    private:
        /// <summary>
        /// The blocks that make up the toy.
        /// </summary>
        std::unordered_map<std::type_index, std::shared_ptr<IBlock>> _blocks;
    };
}