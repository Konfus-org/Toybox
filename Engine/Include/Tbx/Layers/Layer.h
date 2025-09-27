#pragma once
#include "Tbx/DllExport.h"
#include "Tbx/Ids/Uid.h"
#include <string>

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

        virtual void OnAttach() {}
        virtual void OnDetach() {}

        void Update();
        void FixedUpdate();
        void LateUpdate();

    protected:
        virtual void OnUpdate() {}
        virtual void OnFixedUpdate() {}
        virtual void OnLateUpdate() {}

    public:
        std::string Name = "";
        Uid Id = Uid::Generate();
    };
}