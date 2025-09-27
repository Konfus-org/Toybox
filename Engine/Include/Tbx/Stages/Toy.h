#pragma once
#include "Tbx/DllExport.h"
#include "Tbx/Ids/Uid.h"
#include "Tbx/Memory/Refs.h"
#include "Tbx/Stages/Blocks.h"
#include <string>
#include <vector>

namespace Tbx
{
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
        /// <summary>
        /// Creates a toy with the given name.
        /// </summary>
        Toy() = default;

        /// <summary>
        /// Creates a toy with the given name.
        /// </summary>
        Toy(const std::string& name);

        /// <summary>
        /// Updates this toy and recursively updates its children if enabled.
        /// </summary>
        void Update();

    public:
        ToyHandle Handle = {};
        BlockCollection Blocks = {};
        std::vector<Ref<Toy>> Children = {};
        bool Enabled = true;

    protected:
        /// <summary>
        /// Hook called during <see cref="Update"/> before children are updated.
        /// </summary>
        virtual void OnUpdate()
        {
        }
    };
}

