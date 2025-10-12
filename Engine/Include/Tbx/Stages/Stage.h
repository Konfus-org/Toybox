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
            auto toy = Toy::Make<TToy>(std::forward<Args>(args)...);
            Root->Add(toy);
            return toy;
        }

        /// <summary>
        /// Adds an existing toy to the stage.
        /// </summary>
        Ref<Toy> Add(Ref<Toy> toy)
        {
            Root->Add(toy);
            return toy;
        }

        const Ref<Toy> Root = nullptr;

    protected:
        Stage();
    };
}
