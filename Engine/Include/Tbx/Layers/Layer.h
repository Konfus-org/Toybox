#pragma once
#include "Tbx/DllExport.h"
#include "Tbx/Ids/Uid.h"
#include <string>
#include <vector>

namespace Tbx
{
    /// <summary>
    /// An application layer. Used to cleanly add and seperate functionality.
    /// Some examples are a graphics layer, windowing layer, input layer, etc...
    /// </summary>
    class TBX_EXPORT Layer
    {
    public:
        explicit(false) Layer(const std::string& name)
            : Name(name) {}
        virtual ~Layer();

        void AttachTo(std::vector<Layer>& layers);
        void DetachFrom(std::vector<Layer>& layers);

        void Update();

    public:
        std::string Name = "";
        Uid Id = Uid::Generate();

    protected:
        virtual void OnAttach() {}
        virtual void OnDetach() {}
        virtual void OnUpdate() {}
    };
}