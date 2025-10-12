#pragma once
#include "Tbx/DllExport.h"
#include "Tbx/Stages/Toy.h"
#include "Tbx/Memory/Refs.h"
#include <utility>

namespace Tbx
{
    /// <summary>
    /// Represents a collection of toys.
    /// </summary>
    class TBX_EXPORT Stage
    {
    public:
        /// <summary>
        /// Creates a shared stage instance.
        /// </summary>
        static Ref<Stage> Make();

        virtual ~Stage();

        /// <summary>
        /// Updates the toy hierarchy.
        /// </summary>
        void Update();

        /// <summary>
        /// Constructs a toy instance backed by the shared memory pool.
        /// </summary>
        template <typename TToy = Toy, typename... Args>
        Ref<TToy> Add(Args&&... args)
        {
            return Toy::Make<TToy>(std::forward<Args>(args)...);
        }

        const Ref<Toy> Root;

    protected:
        Stage();
    };
}
