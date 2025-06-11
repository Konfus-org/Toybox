#pragma once
#include <Tbx/Utils/DllExport.h>
#include <string>

namespace Tbx
{
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