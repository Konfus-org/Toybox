#pragma once
#include "Tbx/DllExport.h"
#include <string>

namespace Tbx
{
    /// <summary>
    /// An application layer. Used to cleanly add and seperate functionality.
    /// Some examples are a graphics layer, windowing layer, input layer, etc...
    /// </summary>
    class Layer
    {
    public:
        EXPORT explicit(false) Layer(const std::string& name);
        EXPORT virtual ~Layer() = default;

        EXPORT virtual bool IsOverlay() = 0;
        EXPORT virtual void OnAttach() = 0;
        EXPORT virtual void OnDetach() = 0;
        EXPORT virtual void OnUpdate() = 0;

        EXPORT std::string GetName() const;

    private:
        std::string _name;
    };
}