#pragma once
#include "Tbx/DllExport.h"
#include "Tbx/ECS/Toy.h"
#include <functional>
#include <memory>
#include <utility>
#include <vector>

namespace Tbx
{
    using WorldViewIterator = typename std::vector<std::shared_ptr<Toy>>::iterator;
    using ConstWorldViewIterator = typename std::vector<std::shared_ptr<Toy>>::const_iterator;

    /// <summary>
    /// Provides iteration over toys that contain data of the specified block types.
    /// If no types are specified, all toys are included.
    /// </summary>
    template <typename... Ts>
    class WorldView
    {
    public:
        /// <summary>
        /// Creates a view rooted at the given toy.
        /// </summary>
        /// <param name="root">Root toy to search from.</param>
        explicit(false) WorldView(const std::shared_ptr<Toy>& root)
        {
            if (root)
            {
                BuildViewVector(root);
            }
        }

        /// <summary>Returns an iterator to the first toy in the view.</summary>
        WorldViewIterator begin() { return _viewVector.begin(); }

        /// <summary>Returns an iterator one past the last toy in the view.</summary>
        WorldViewIterator end() { return _viewVector.end(); }

        /// <summary>Returns a const iterator to the first toy in the view.</summary>
        ConstWorldViewIterator begin() const { return _viewVector.begin(); }

        /// <summary>Returns a const iterator one past the last toy in the view.</summary>
        ConstWorldViewIterator end() const { return _viewVector.end(); }

        /// <summary>Returns a const iterator to the first toy in the view.</summary>
        ConstWorldViewIterator cbegin() const { return _viewVector.cbegin(); }

        /// <summary>Returns a const iterator one past the last toy in the view.</summary>
        ConstWorldViewIterator cend() const { return _viewVector.cend(); }

    private:
        /// <summary>
        /// Returns true if the toy matches the view's type filter:
        /// - With no Ts, always true (all toys).
        /// - With Ts..., toy must have at least one of the specified blocks.
        /// </summary>
        static bool Matches(const std::shared_ptr<Toy>& toy)
        {
            if constexpr (sizeof...(Ts) == 0)
            {
                return true; // no filter => include all
            }
            else
            {
                return (toy->template HasBlock<Ts>() || ...); // any-of
            }
        }

        void BuildViewVector(const std::shared_ptr<Toy>& toy)
        {
            if (Matches(toy))
            {
                _viewVector.push_back(toy);
            }
            for (const auto& child : toy->GetChildren())
            {
                BuildViewVector(child);
            }
        }

        std::vector<std::shared_ptr<Toy>> _viewVector = {};
    };

    /// <summary>
    /// A view that includes all toys in the hierarchy.
    /// </summary>
    using FullWorldView = WorldView<>;
}