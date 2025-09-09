#pragma once
#include "Tbx/DllExport.h"
#include "Tbx/TBS/Toy.h"
#include <functional>
#include <memory>
#include <utility>
#include <vector>

namespace Tbx
{
    /// <summary>
    /// Provides iteration over toys that contain data of type <typeparamref name="T"/>.
    /// </summary>
    template <typename T>
    class WorldView
    {
    public:
        using iterator = typename std::vector<std::shared_ptr<Toy>>::iterator;
        using const_iterator = typename std::vector<std::shared_ptr<Toy>>::const_iterator;

        /// <summary>
        /// Creates a view rooted at the given toy.
        /// </summary>
        /// <param name="root">Root toy to search from.</param>
        explicit WorldView(const std::shared_ptr<Toy>& root)
        {
            if (root)
            {
                Build(root);
            }
        }

        /// <summary>
        /// Returns an iterator to the first toy in the view.
        /// </summary>
        iterator begin()
        {
            return _toys.begin();
        }

        /// <summary>
        /// Returns an iterator one past the last toy in the view.
        /// </summary>
        iterator end()
        {
            return _toys.end();
        }

        /// <summary>
        /// Returns a const iterator to the first toy in the view.
        /// </summary>
        const_iterator begin() const
        {
            return _toys.begin();
        }

        /// <summary>
        /// Returns a const iterator one past the last toy in the view.
        /// </summary>
        const_iterator end() const
        {
            return _toys.end();
        }

    private:
        void Build(const std::shared_ptr<Toy>& toy)
        {
            if (toy->template HasBlock<T>())
            {
                _toys.push_back(toy);
            }
            for (const auto& child : toy->GetChildren())
            {
                Build(child);
            }
        }

        std::vector<std::shared_ptr<Toy>> _toys = {};
    };

    /// <summary>
    /// Function invoked for systems that operate on toys containing data of type <typeparamref name="T"/>.
    /// </summary>
    template <typename T>
    using WorldSystem = std::function<void(WorldView<T>)>;

    /// <summary>
    /// Manages a hierarchy of toys and executes registered systems.
    /// </summary>
    class World
    {
    public:
        /// <summary>
        /// Initializes a new instance of <see cref="World"/> with a root toy.
        /// </summary>
        EXPORT World();

        /// <summary>
        /// Gets the root toy of the hierarchy.
        /// </summary>
        /// <returns>The root toy.</returns>
        EXPORT std::shared_ptr<Toy> GetRoot() const;

        /// <summary>
        /// Registers a system to operate on toys containing data of type <typeparamref name="T"/>.
        /// </summary>
        /// <typeparam name="T">Type of data required by the system.</typeparam>
        /// <param name="system">The system callback to register.</param>
        template <typename T>
        void AddSystem(WorldSystem<T> system)
        {
            _systems.push_back([this, system = std::move(system)]()
            {
                system(WorldView<T>(_root));
            });
        }

        /// <summary>
        /// Updates all registered systems and then updates the toy hierarchy.
        /// </summary>
        EXPORT void Update();

    private:
        std::shared_ptr<Toy> _root = {};
        std::vector<std::function<void()>> _systems = {};
    };
}

