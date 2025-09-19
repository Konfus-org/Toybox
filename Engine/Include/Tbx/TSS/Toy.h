#pragma once
#include "Tbx/DllExport.h"
#include "Tbx/Ids/Uid.h"
#include <memory>
#include <string>
#include <string_view>
#include <typeindex>
#include <unordered_map>
#include <vector>
#include <any>

namespace Tbx
{
    /// <summary>
    /// Identifies a toy via a unique ID and name.
    /// </summary>
    struct EXPORT ToyHandle : public UsesUid
    {
    public:
        /// <summary>
        /// A default, invalid toy handle
        /// </summary>
        ToyHandle() : UsesUid(Consts::Invalid::Uid)
        {
        }

        /// <summary>
        /// Creates a handle with the specified name.
        /// </summary>
        explicit(false) ToyHandle(const std::string& name)
            : _name(name)
        {
        }

        /// <summary>
        /// Gets the name associated with this handle.
        /// </summary>
        /// <returns>The handle name.</returns>
        const std::string& GetName() const
        {
            return _name;
        }

    private:
        std::string _name;
    };

    /// <summary>
    /// Represents a toy in a hierarchy with arbitrary typed blocks.
    /// </summary>
    class Toy : public std::enable_shared_from_this<Toy>
    {
    public:
        /// <summary>
        /// Initializes a new instance of <see cref="Toy"/> with an optional name.
        /// </summary>
        EXPORT Toy(const std::string& name = "");

        /// <summary>
        /// Creates a child on this toy with the given name.
        /// </summary>
        /// <param name="child">The child toy to add.</param>
        EXPORT std::shared_ptr<Toy> EmplaceChild(const std::string& name);

        /// <summary>
        /// Adds a child to this toy and sets the child's parent.
        /// </summary>
        /// <param name="child">The child toy to add.</param>
        EXPORT std::shared_ptr<Toy> AddChild(const std::shared_ptr<Toy>& child);

        /// <summary>
        /// Removes a child from this toy and clears its parent.
        /// </summary>
        /// <param name="child">The child toy to remove.</param>
        EXPORT void RemoveChild(const std::shared_ptr<Toy>& child);

        /// <summary>
        /// Retrieves a direct child matching the given handle.
        /// </summary>
        /// <param name="handle">Handle identifying the child.</param>
        /// <returns>The child toy if found; otherwise, <c>nullptr</c>.</returns>
        EXPORT std::shared_ptr<Toy> GetChild(const ToyHandle& handle) const;

        /// <summary>
        /// Finds the first direct child with the specified name.
        /// </summary>
        /// <param name="name">Name of the child to find.</param>
        /// <returns>The child toy if found; otherwise, <c>nullptr</c>.</returns>
        EXPORT std::shared_ptr<Toy> FindChild(std::string_view name) const;

        /// <summary>
        /// Returns a const reference to the list of children.
        /// </summary>
        /// <returns>The list of child toys.</returns>
        EXPORT const std::vector<std::shared_ptr<Toy>>& GetChildren() const;

        /// <summary>
        /// Sets the parent for this toy.
        /// </summary>
        /// <param name="parent">Shared pointer to the parent toy.</param>
        EXPORT void SetParent(const std::shared_ptr<Toy>& parent);

        /// <summary>
        /// Gets the parent toy.
        /// </summary>
        /// <returns>Shared pointer to the parent toy.</returns>
        EXPORT std::shared_ptr<Toy> GetParent() const;

        /// <summary>
        /// Gets the name of this toy.
        /// </summary>
        /// <returns>The toy name.</returns>
        EXPORT const std::string& GetName() const;

        /// <summary>
        /// Gets the handle uniquely identifying this toy.
        /// </summary>
        /// <returns>The toy handle.</returns>
        EXPORT const ToyHandle& GetHandle() const;

        /// <summary>
        /// Enables or disables this toy.
        /// </summary>
        /// <param name="enabled">True to enable the toy; otherwise, false.</param>
        EXPORT void SetEnabled(bool enabled);

        /// <summary>
        /// Determines whether this toy is enabled.
        /// </summary>
        /// <returns>True if enabled; otherwise, false.</returns>
        EXPORT bool IsEnabled() const;

        /// <summary>
        /// Updates this toy and recursively updates its children if enabled.
        /// </summary>
        EXPORT void Update();

        /// <summary>
        /// Constructs data of type <typeparamref name="T"/> and stores it within the toy.
        /// </summary>
        /// <typeparam name="T">Type of the block to construct.</typeparam>
        /// <param name="args">Arguments forwarded to the block constructor.</param>
        /// <returns>Reference to the constructed block.</returns>
        template <typename T, typename... Args>
        T& EmplaceBlock(Args&&... args)
        {
            const auto typeIndex = std::type_index(typeid(std::remove_cvref_t<T>));
            auto& block = _blocks[typeIndex];
            block = T(std::forward<Args>(args)...);
            return std::any_cast<T&>(block);
        }

        /// <summary>
        /// Retrieves stored data of type <typeparamref name="T"/>.
        /// </summary>
        /// <typeparam name="T">Type of the block to retrieve.</typeparam>
        /// <returns>Reference to the block.</returns>
        template <typename T>
        T& GetBlock()
        {
            const auto typeIndex = std::type_index(typeid(std::remove_cvref_t<T>));
            auto& block = _blocks.at(typeIndex);
            return std::any_cast<T&>(block);
        }

        /// <summary>
        /// Retrieves stored data of type <typeparamref name="T"/>.
        /// </summary>
        /// <typeparam name="T">Type of the block to retrieve.</typeparam>
        /// <returns>Const reference to the block.</returns>
        template <typename T>
        const T& GetBlock() const
        {
            const auto typeIndex = std::type_index(typeid(std::remove_cvref_t<T>));
            const auto& block = _blocks.at(typeIndex);
            return std::any_cast<const T&>(block);
        }

        /// <summary>
        /// Determines whether data of type <typeparamref name="T"/> is stored on this toy.
        /// </summary>
        /// <typeparam name="T">Type of the block to check.</typeparam>
        /// <returns>True if the block exists; otherwise, false.</returns>
        template <typename T>
        bool HasBlock() const
        {
            const auto typeIndex = std::type_index(typeid(std::remove_cvref_t<T>));
            return _blocks.find(typeIndex) != _blocks.end();
        }

    protected:
        /// <summary>
        /// Hook called during <see cref="Update"/> before children are updated.
        /// </summary>
        virtual void OnUpdate()
        {
        }

    private:
        bool _enabled = true;
        ToyHandle _handle = {};
        std::weak_ptr<Toy> _parent = {};
        std::vector<std::shared_ptr<Toy>> _children = {};
        std::unordered_map<std::type_index, std::any> _blocks = {};
    };
}

