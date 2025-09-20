#pragma once
#include "Tbx/DllExport.h"
#include "Tbx/TSS/Toy.h"
#include "Tbx/Memory/Refs.h"
#include <memory>
#include <vector>

namespace Tbx
{
    using StageViewIterator = typename std::vector<Ref<Toy>>::iterator;
    using ConstStageViewIterator = typename std::vector<Ref<Toy>>::const_iterator;

    /// <summary>
    /// Provides iteration over toys that contain data of the specified block types.
    /// If no types are specified, all toys are included.
    /// </summary>
    template <typename... Ts>
    class StageView
    {
    public:
        /// <summary>
        /// Creates a view rooted at the given toy.
        /// </summary>
        /// <param name="root">Root toy to search from.</param>
        explicit(false) StageView(const Ref<Toy>& root)
        {
            if (root)
            {
                BuildViewVector(root);
            }
        }

        /// <summary>Returns an iterator to the first toy in the view.</summary>
        StageViewIterator begin() { return _viewVector.begin(); }

        /// <summary>Returns an iterator one past the last toy in the view.</summary>
        StageViewIterator end() { return _viewVector.end(); }

        /// <summary>Returns a const iterator to the first toy in the view.</summary>
        ConstStageViewIterator begin() const { return _viewVector.begin(); }

        /// <summary>Returns a const iterator one past the last toy in the view.</summary>
        ConstStageViewIterator end() const { return _viewVector.end(); }

        /// <summary>Returns a const iterator to the first toy in the view.</summary>
        ConstStageViewIterator cbegin() const { return _viewVector.cbegin(); }

        /// <summary>Returns a const iterator one past the last toy in the view.</summary>
        ConstStageViewIterator cend() const { return _viewVector.cend(); }

    private:
        /// <summary>
        /// Returns true if the toy matches the view's type filter:
        /// - With no Ts, always true (all toys).
        /// - With Ts..., toy must have at least one of the specified blocks.
        /// </summary>
        static bool Matches(const Ref<Toy>& toy)
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

        void BuildViewVector(const Ref<Toy>& toy)
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

        std::vector<Ref<Toy>> _viewVector = {};
    };

    /// <summary>
    /// A view that includes all toys in the hierarchy.
    /// </summary>
    using FullStageViewView = StageView<>;
}